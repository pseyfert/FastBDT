/**
 * Thomas Keck 2014
 */
#include "MethodFastBDT.h"
#include <vector>
#include <fstream>

#include "Riostream.h"
#include "TRandom3.h"
#include "TMath.h"
#include "TObjString.h"

#include "TMVA/ClassifierFactory.h"
#include "TMVA/Tools.h"
#include "TMVA/Timer.h"
#include "TMVA/Ranking.h"
#include "TMVA/Results.h"
#include "TMVA/ResultsMulticlass.h"

#include "FastBDT_IO.h"

ClassImp(TMVA::MethodFastBDT)

using namespace FastBDT;

#if ROOT_VERSION_CODE >= ROOT_VERSION(6,8,0)
TMVA::MethodFastBDT::MethodFastBDT( const TString& jobName,
                            const TString& methodTitle,
                            DataSetInfo& theData,
                            const TString& theOption ) :
   TMVA::MethodBase( jobName, Types::kPlugins, methodTitle, theData, theOption)
#else
TMVA::MethodFastBDT::MethodFastBDT( const TString& jobName,
                            const TString& methodTitle,
                            DataSetInfo& theData,
                            const TString& theOption,
                            TDirectory* theTargetDir ) :
   TMVA::MethodBase( jobName, Types::kPlugins, methodTitle, theData, theOption, theTargetDir )
#endif
   , fNTrees(0)
   , fShrinkage(0)
   , fRandRatio(0)
   , fsPlot(false)
   , transform2probability(false)
   , useWeightedFeatureBinning(false)
   , fPurityTransformation(0)
   , useEquidistantFeatureBinning(false)
   , fNCutLevel(0)
   , fNTreeLayers(0)
   , fForest(NULL)
{
}

#if ROOT_VERSION_CODE >= ROOT_VERSION(6,8,0)
TMVA::MethodFastBDT::MethodFastBDT( DataSetInfo& theData,
                            const TString& theWeightFile)
   : TMVA::MethodBase( Types::kPlugins, theData, theWeightFile)
#else
TMVA::MethodFastBDT::MethodFastBDT( DataSetInfo& theData,
                            const TString& theWeightFile,
                            TDirectory* theTargetDir )
   : TMVA::MethodBase( Types::kPlugins, theData, theWeightFile, theTargetDir )
#endif
   , fNTrees(0)
   , fShrinkage(0)
   , fRandRatio(0)
   , fsPlot(false)
   , transform2probability(false)
   , useWeightedFeatureBinning(false)
   , fPurityTransformation(0)
   , useEquidistantFeatureBinning(false)
   , fNCutLevel(0)
   , fNTreeLayers(0)
   , fForest(NULL)
{
}

TMVA::MethodFastBDT::~MethodFastBDT( void )
{
   if( fForest != NULL )
     delete fForest;
}

Bool_t TMVA::MethodFastBDT::HasAnalysisType( Types::EAnalysisType type, UInt_t numberClasses, UInt_t)
{
   if (type == Types::kClassification && numberClasses == 2) return kTRUE;
   return kFALSE;
}

void TMVA::MethodFastBDT::DeclareOptions()
{

   DeclareOptionRef(fNTrees=100, "NTrees", FastBDT_nTrees_String);
   DeclareOptionRef(fNTreeLayers=3,"NTreeLayers",FastBDT_nLevels_String);
   DeclareOptionRef(fNCutLevel=8, "NCutLevel", FastBDT_nCuts_String);
   DeclareOptionRef(fShrinkage=1.0, "Shrinkage", FastBDT_shrinkage_String);
   DeclareOptionRef(fRandRatio=1.0, "RandRatio", FastBDT_randRatio_String);
   DeclareOptionRef(fsPlot=false, "sPlot", "Keep signal and background event pairs together during stochastic bagging, should improve an sPlot training, but frankly said: There was no difference in my tests");
   DeclareOptionRef(fPurityTransformation=0, "purityTransformation", "Transform the variables into purity space before building the tree, this will slow down the training a little bit, and the application a lot (Not implemented in the TMVA method yet, only in FastBDT)");
   DeclareOptionRef(standaloneWeightfileName=TString(""), "standaloneWeightfileName", "Write out a standalone weightfile for FastBDT in addition to the TMVA weightfile.");
   DeclareOptionRef(transform2probability=true, "transform2probability", "Use sigmoid function to transform output to probability");
   DeclareOptionRef(useWeightedFeatureBinning=false, "useWeightedFeatureBinning", "Use weighted feature binning for equal frequency binning.");
   DeclareOptionRef(useEquidistantFeatureBinning=false, "useEquidistantFeatureBinning", "Use equidistant binning instead of equal frequency binning.");
}

void TMVA::MethodFastBDT::ProcessOptions()
{
    // Here is the right place for additional processing of the given options.
    // See other TMVA Methods for examples.
}

void TMVA::MethodFastBDT::Init()
{
   // Common initialisation with defaults for the FastBDT-Method
   fNTrees         = 100;
   fNTreeLayers        = 3;
   fNCutLevel          = 100;
   fShrinkage       = 1.0;
   fRandRatio       = 1.0;
   fsPlot           = false;
   fPurityTransformation = 0;
   transform2probability = true;
   useWeightedFeatureBinning = false;
   useEquidistantFeatureBinning = false;
}


void TMVA::MethodFastBDT::Reset()
{
   // Reset the method, as if it had just been instantiated (forget all training etc.)
   if( fForest != NULL )   
     delete fForest;
   fForest = NULL;
}


void TMVA::MethodFastBDT::Train()
{

  Log() << kINFO << "Start FastBDT training" << Endl;
  Data()->SetCurrentType(Types::kTraining);
  UInt_t nEvents = Data()->GetNTrainingEvents();
  UInt_t nFeatures = GetNvar();
  
  // First thing is to read out the training data from Data()
  // into an EventSample object.
  std::vector<FastBDT::FeatureBinning<double>> featureBinnings;
  std::vector<unsigned int> nBinningLevels;
 
  Log() << kINFO << "Create Binning" << Endl;
  if(useEquidistantFeatureBinning) {
      for(unsigned int iFeature = 0; iFeature < nFeatures; ++iFeature) {
          std::vector<double> feature(nEvents,0);
          for (unsigned int iEvent=0; iEvent<nEvents; iEvent++) {
             feature[iEvent] = GetTrainingEvent(iEvent)->GetValue(iFeature);
          }
          featureBinnings.push_back( EquidistantFeatureBinning<double>(fNCutLevel, feature ) );
          nBinningLevels.push_back(fNCutLevel);
      }

  } else if(useWeightedFeatureBinning) {

      unsigned int total_signal_events = 0;
      double total_signal_weight = 0;
      unsigned int total_bckgrd_events = 0;
      double total_bckgrd_weight = 0;
      for (unsigned int iEvent=0; iEvent<nEvents; iEvent++) {
         if(DataInfo().IsSignal(GetTrainingEvent(iEvent))) {
            total_signal_events++;
            total_signal_weight += GetTrainingEvent(iEvent)->GetWeight();
         } else {
            total_bckgrd_events++;
            total_bckgrd_weight +=GetTrainingEvent(iEvent)->GetWeight();
         }
      }
      double signal_correction = (2*total_bckgrd_weight) / (total_signal_weight + total_bckgrd_weight);
      double bckgrd_correction = (2*total_signal_weight) / (total_signal_weight + total_bckgrd_weight);

      std::vector<FastBDT::Weight> weights(nEvents,0);
      for (unsigned int iEvent=0; iEvent<nEvents; iEvent++) {
         if(DataInfo().IsSignal(GetTrainingEvent(iEvent))) {
            weights[iEvent] = GetTrainingEvent(iEvent)->GetWeight() * signal_correction;
         } else {
            weights[iEvent] = GetTrainingEvent(iEvent)->GetWeight() * bckgrd_correction;
         }
      }
      for(unsigned int iFeature = 0; iFeature < nFeatures; ++iFeature) {
          std::vector<double> feature(nEvents,0);
          for (unsigned int iEvent=0; iEvent<nEvents; iEvent++) {
             feature[iEvent] = GetTrainingEvent(iEvent)->GetValue(iFeature);
          }
          featureBinnings.push_back( WeightedFeatureBinning<double>(fNCutLevel, feature, weights ) );
          nBinningLevels.push_back(fNCutLevel);

          auto v = featureBinnings.back().GetBinning();
          std::sort(v.begin(), v.end());
      }

  } else {
      for(unsigned int iFeature = 0; iFeature < nFeatures; ++iFeature) {
          std::vector<double> feature(nEvents,0);
          for (unsigned int iEvent=0; iEvent<nEvents; iEvent++) {
             feature[iEvent] = GetTrainingEvent(iEvent)->GetValue(iFeature);
          }
          featureBinnings.push_back( FeatureBinning<double>(fNCutLevel, feature ) );
          nBinningLevels.push_back(fNCutLevel);
      }
  }

  unsigned int nEventsPruned = 0;
  for(unsigned int iEvent = 0; iEvent < nEvents; ++iEvent) {
     if(std::abs(GetTrainingEvent(iEvent)->GetWeight()) < 1e-8) {
       std::cerr << "Removed event with extremly small value" << std::endl;
       continue;
     }
     nEventsPruned++;
  }

  Log() << kINFO << "Build EventSample" << Endl;
  EventSample eventSample(nEventsPruned, nFeatures, nBinningLevels);

  for(unsigned int iEvent = 0; iEvent < nEvents; ++iEvent) {
      std::vector<unsigned int> bins(nFeatures);
      auto *event = GetTrainingEvent(iEvent);
      if(std::abs(event->GetWeight()) < 1e-8) {
        continue;
      }
      for(unsigned int iFeature = 0; iFeature < nFeatures; ++iFeature) {
          bins[iFeature] = featureBinnings[iFeature].ValueToBin( event->GetValue(iFeature) );
      }
      auto weight = event->GetWeight();
      eventSample.AddEvent(bins, weight, DataInfo().IsSignal(event));
  }
 
  // Create the forest, this also trains the whole forest immediatly
  Log() << kINFO << "Train forest" << Endl;
  ForestBuilder builder(eventSample, fNTrees, fShrinkage, fRandRatio, fNTreeLayers, fsPlot);
  
  Log() << kINFO << "Remove feature binning from trained forest" << Endl;
  fForest = new Forest<double>( builder.GetShrinkage(), builder.GetF0(), transform2probability);
  for( auto tree : builder.GetForest() )
      fForest->AddTree(removeFeatureBinningTransformationFromTree(tree, featureBinnings));
  
  Log() << kINFO << "Finished training" << Endl;

  if( standaloneWeightfileName.Length() > 0 ) {
    Log() << kINFO << "Write out additional standalone weightfile to " << standaloneWeightfileName << Endl;
    std::fstream file(standaloneWeightfileName.Data(), std::ios_base::out | std::ios_base::trunc);
    if(file.is_open()) {
        file << (*fForest) << std::endl;
        file.close();
    } else {
        Log() << kINFO << "Failed to open standalone weightfile for writing" << Endl;
    }
  }

}

/**
 * This template saves a vector to a xml file. It creates a new child with the given name
 * and stores the vector under this child.
 * @param parent the parent xml node
 * @param name the name of the child node which contains the vector
 * @param vector the vector which shall be stored
 */
template<class T>
void SaveVectorToXML(void *parent, std::string name, const std::vector<T> &vector) {

   // Create a new child
   void *vecxml = TMVA::gTools().AddChild( parent, name.c_str() );
   // Attach the size of the vector as an attribute to this child
   TMVA::gTools().AddAttr( vecxml, "Size", vector.size() );
   // Store all entries of the vector by attaching additional child-entries
   // The index and the value of every entry of the vector are stored as attributes.
   for (unsigned int i = 0; i < vector.size(); ++i) {
      void *entryxml = TMVA::gTools().AddChild( vecxml, "Entry" );
      TMVA::gTools().AddAttr( entryxml, "Index", i );
      TMVA::gTools().AddAttr( entryxml, "Value", vector[i] );
   }

}

/**
 * This template reads a vector from a xml file. 
 * It reads out the child with the given name and fill the given vector with
 * the entries of this child.
 * @param parent the parent xml node
 * @param name the name of the child node which contains the vector
 * @param vector the vector in which the values read from the child are stored
 */
template<class T>
void ReadVectorFromXML(void *parent, std::string name, std::vector<T> &vector) {

   // Get the child with the name of the vector
   void* vecxml = TMVA::gTools().GetChild(parent, name.c_str());

   // Read out the size of the vector and resize the vector to the correct size
   unsigned int size;
   TMVA::gTools().ReadAttr( vecxml, "Size", size );
   vector.resize(size);

   // Loop over all the entries under the child and read out the index and the value
   // of the entries, and fill the vector with them.
   void* entryxml = TMVA::gTools().GetChild(vecxml, "Entry");
   while (entryxml) {
      unsigned int i;
      T v;
      TMVA::gTools().ReadAttr( entryxml, "Index", i );
      TMVA::gTools().ReadAttr( entryxml, "Value", v);
      vector[i] = v;
      entryxml = TMVA::gTools().GetNextChild(entryxml);
   }

}


void TMVA::MethodFastBDT::AddWeightsXMLTo( void* parent ) const
{
   // write weights to XML
   Log() << kINFO << "Write FastBDT weightfile to xml" << Endl;
   void* wght = TMVA::gTools().AddChild(parent, "Weights");

   TMVA::gTools().AddAttr( wght, "Version", 2 );
   TMVA::gTools().AddAttr( wght, "NTrees", fNTrees );
   TMVA::gTools().AddAttr( wght, "Shrinkage", fShrinkage );
   TMVA::gTools().AddAttr( wght, "NCuts", fNCutLevel );
   TMVA::gTools().AddAttr( wght, "NLevels", fNTreeLayers );
   TMVA::gTools().AddAttr( wght, "RandRatio", fRandRatio );
   TMVA::gTools().AddAttr( wght, "sPlot", fsPlot );
   TMVA::gTools().AddAttr( wght, "transform2probability", transform2probability );
   TMVA::gTools().AddAttr( wght, "F0", fForest->GetF0() );

   auto &forest = fForest->GetForest();
   for(unsigned int i = 0; i < forest.size(); ++i) {
      void* trxml = TMVA::gTools().AddChild(wght, "Tree");
      TMVA::gTools().AddAttr( trxml, "iTree", i );
      
      std::vector<unsigned int> cut_features;
      std::vector<double> cut_indexes;
      std::vector<bool> cut_valids;
      std::vector<double> cut_gains;
      for( auto& cut : forest[i].GetCuts() ) {
        cut_features.push_back( cut.feature );
        cut_indexes.push_back( cut.index );
        cut_valids.push_back( cut.valid );
        cut_gains.push_back( cut.gain );
      }

      SaveVectorToXML(trxml, "CutFeatures", cut_features);
      SaveVectorToXML(trxml, "CutIndexes", cut_indexes);
      SaveVectorToXML(trxml, "CutValids", cut_valids);
      SaveVectorToXML(trxml, "CutGains", cut_gains);
      SaveVectorToXML(trxml, "BoostWeights", forest[i].GetBoostWeights());
      SaveVectorToXML(trxml, "Purities", forest[i].GetPurities());
      SaveVectorToXML(trxml, "NEntries", forest[i].GetNEntries());
   }

}

void TMVA::MethodFastBDT::ReadWeightsFromXML(void* parent) {

   Reset();
   Log() << kINFO << "Read FastBDT weightfile from xml" << Endl;
   if(TMVA::gTools().HasAttr( parent, "Version" )) {
     unsigned int version = 0;
     TMVA::gTools().ReadAttr( parent, "Version", version );
     if(version == 2) {
      Log() << kINFO << "Loading newest FastBDT weightfile Version 2." << Endl;
      TMVA::MethodFastBDT::ReadWeightsFromXML_V2(parent);
     } else {
        Log() << kFATAL << "Unkown FastBDT weightfile Version " << std::to_string(version) << ". Sorry I have no idea how to load this" << Endl;
     }
   } else {
   Log() << kINFO << "Loading deprecated FastBDT weightfile Version 1." << Endl;
   Log() << kINFO << "This should be save, however think about retraining your classifier, so you can profit from improvements in the algorithm." << Endl;
	 TMVA::MethodFastBDT::ReadWeightsFromXML_V1(parent);
   } 

}

void TMVA::MethodFastBDT::ReadWeightsFromXML_V2(void* parent) {

   Log() << kINFO << "Read FastBDT weightfile from xml version 2" << Endl;
   TMVA::gTools().ReadAttr( parent, "NTrees", fNTrees );
   TMVA::gTools().ReadAttr( parent, "Shrinkage", fShrinkage );
   TMVA::gTools().ReadAttr( parent, "NCuts", fNCutLevel );
   TMVA::gTools().ReadAttr( parent, "NLevels", fNTreeLayers );
   TMVA::gTools().ReadAttr( parent, "RandRatio", fRandRatio );
   TMVA::gTools().ReadAttr( parent, "sPlot", fsPlot );
   TMVA::gTools().ReadAttr( parent, "transform2probability", transform2probability );

   double F0;
   TMVA::gTools().ReadAttr( parent, "F0", F0 );
   fForest = new Forest<double>(fShrinkage, F0, transform2probability);

   void* trxml = TMVA::gTools().GetChild(parent, "Tree");
   while (trxml) {
      // Read the tree number, this value isn't used currently
      unsigned int iTree;
      TMVA::gTools().ReadAttr( trxml, "iTree", iTree );
      // Read the weights
      std::vector<unsigned int> cut_features;
      std::vector<double> cut_indexes;
      std::vector<bool> cut_valids;
      std::vector<double> cut_gains;
      std::vector<FastBDT::Weight> boost_weights;
      std::vector<FastBDT::Weight> purities;
      std::vector<FastBDT::Weight> nEntries;
      ReadVectorFromXML(trxml, "CutFeatures", cut_features);
      ReadVectorFromXML(trxml, "CutIndexes", cut_indexes);
      ReadVectorFromXML(trxml, "CutValids", cut_valids);
      ReadVectorFromXML(trxml, "CutGains", cut_gains);
      ReadVectorFromXML( trxml, "BoostWeights", boost_weights);
      ReadVectorFromXML( trxml, "Purities", purities);
      ReadVectorFromXML( trxml, "NEntries", nEntries);

      std::vector<Cut<double>> cuts;
      for(unsigned int i = 0; i < cut_features.size(); ++i) {
        Cut<double> cut;
        cut.feature = cut_features[i];
        cut.index = cut_indexes[i];
        cut.valid = cut_valids[i];
        cut.gain = cut_gains[i];
        cuts.push_back(cut);
      }
      fForest->AddTree(Tree<double>(cuts, nEntries, purities, boost_weights));

      trxml = TMVA::gTools().GetNextChild(trxml, "Tree");
   }

}

void TMVA::MethodFastBDT::ReadWeightsFromXML_V1(void* parent) {

   Log() << kINFO << "Read FastBDT weightfile from xml version 1" << Endl;
   void* binxml = TMVA::gTools().GetChild(parent, "Binning");

   std::vector<FastBDT::FeatureBinning<double>> featureBinnings;

   while (binxml) {
      unsigned int nLevels;
      TMVA::gTools().ReadAttr( binxml, "NLevels", nLevels );
      std::vector<double> bins;
      ReadVectorFromXML(binxml, "bins", bins);
      featureBinnings.push_back(FeatureBinning<double>(nLevels, bins));
      binxml = TMVA::gTools().GetNextChild(binxml, "Binning");
   }

   TMVA::gTools().ReadAttr( parent, "NTrees", fNTrees );
   TMVA::gTools().ReadAttr( parent, "Shrinkage", fShrinkage );
   TMVA::gTools().ReadAttr( parent, "NCuts", fNCutLevel );
   TMVA::gTools().ReadAttr( parent, "NLevels", fNTreeLayers );
   TMVA::gTools().ReadAttr( parent, "RandRatio", fRandRatio );
   TMVA::gTools().ReadAttr( parent, "sPlot", fsPlot );
   
   transform2probability = true;

   double F0;
   TMVA::gTools().ReadAttr( parent, "F0", F0 );
   fForest = new Forest<double>(fShrinkage, F0, transform2probability);

   void* trxml = TMVA::gTools().GetChild(parent, "Tree");
   while (trxml) {
      // Read the tree number, this value isn't used currently
      unsigned int iTree;
      TMVA::gTools().ReadAttr( trxml, "iTree", iTree );
      // Read the weights
      std::vector<unsigned int> cut_features;
      std::vector<unsigned int> cut_indexes;
      std::vector<bool> cut_valids;
      std::vector<double> cut_gains;
      std::vector<FastBDT::Weight> boost_weights;
      std::vector<FastBDT::Weight> purities;
      std::vector<FastBDT::Weight> nEntries;
      ReadVectorFromXML(trxml, "CutFeatures", cut_features);
      ReadVectorFromXML(trxml, "CutIndexes", cut_indexes);
      ReadVectorFromXML(trxml, "CutValids", cut_valids);
      ReadVectorFromXML(trxml, "CutGains", cut_gains);
      ReadVectorFromXML( trxml, "BoostWeights", boost_weights);
      ReadVectorFromXML( trxml, "Purities", purities);
      ReadVectorFromXML( trxml, "NEntries", nEntries);

      std::vector<Cut<unsigned int>> cuts;
      for(unsigned int i = 0; i < cut_features.size(); ++i) {
        Cut<unsigned int> cut;
        cut.feature = cut_features[i];
        cut.index = cut_indexes[i];
        cut.valid = cut_valids[i];
        cut.gain = cut_gains[i];
        cuts.push_back(cut);
      }
      Tree<unsigned int> tree(cuts, nEntries, purities, boost_weights);
      fForest->AddTree(removeFeatureBinningTransformationFromTree(tree, featureBinnings));

      trxml = TMVA::gTools().GetNextChild(trxml, "Tree");
   }

}

void  TMVA::MethodFastBDT::ReadWeightsFromStream( std::istream&)
{
      Log() << kFATAL << "Reading Weights From Stream is currently not supported" << Endl;
}

Double_t TMVA::MethodFastBDT::GetMvaValue( Double_t*, Double_t*){
 
  // First get the current event and store it in a vector 
  std::vector<double> bins(GetNvar(),0);
  const Event* ev = Data()->GetEvent();
  for (unsigned int iFeature = 0; iFeature < GetNvar(); iFeature++) {
    bins[iFeature] = ev->GetValue(iFeature);
  }
  return fForest->Analyse(bins);

}

const TMVA::Ranking* TMVA::MethodFastBDT::CreateRanking()
{
   Log() << kINFO << "Create FastBDT ranking" << Endl;
   // Compute ranking of input variables
   fRanking = new TMVA::Ranking( GetName(), "Variable Importance" );
   std::map<unsigned int, double> ranking = fForest->GetVariableRanking();
   for(auto &pair : ranking ) {
        fRanking->AddRank( TMVA::Rank( GetInputLabel(pair.first) , pair.second) );
   }
   return fRanking;
}

void TMVA::MethodFastBDT::GetHelpMessage() const
{
   Log() << kINFO << "FastBDT: See https://github.com/thomaskeck/FastBDT" << Endl;
   Log() << kINFO << "Cite https://arxiv.org/abs/1609.06119" << Endl;
}


void createCStatementsForCuts(std::ostream& fout, const std::vector<Cut<double>> &cuts, const std::vector<Weight> &boostWeights, unsigned int iCut) {
      
  if(iCut < cuts.size() && cuts[iCut].valid) {
    fout << " if( std::isnan(inputValues[" << cuts[iCut].feature << "]) ) { " << std::endl;
    fout << "   F += " << boostWeights[iCut] << ";" << std::endl;
    fout << "  } " << std::endl;
    fout << "  else if(inputValues[" << cuts[iCut].feature << "] >= " << cuts[iCut].index << ") { " << std::endl;
    createCStatementsForCuts(fout, cuts, boostWeights, 2*(iCut+1) + 1 - 1);
    fout << "  } else { " << std::endl;
    createCStatementsForCuts(fout, cuts, boostWeights, 2*(iCut+1) - 1);
    fout << "  } " << std::endl;
  } else {
    fout << "   F += " << boostWeights[iCut] << ";" << std::endl; 
  }

}

void TMVA::MethodFastBDT::MakeClassSpecific( std::ostream& fout, const TString& className ) const
{
   fout << "};" << std::endl << std::endl;
   fout << "double " << className << "::GetMvaValue__( const std::vector<double>& inputValues ) const" << std::endl;
   fout << "{" << std::endl;
   fout << "   double F = " << (fForest->GetF0() / fForest->GetShrinkage()) << ";" << std::endl;
   for(auto &tree : fForest->GetForest()) {
      createCStatementsForCuts(fout, tree.GetCuts(), tree.GetBoostWeights(), 0);
    }
    fout << " F *= " << std::to_string(fForest->GetShrinkage()) << ";" << std::endl;
    if(fForest->GetTransform2Probability()) {
        fout << " F = 1.0/(1.0+std::exp(-2*F)); " << std::endl;
    }
   fout << "   return F;" << std::endl;
   fout << "};" << std::endl << std::endl;
   fout << "void " << className << "::Initialize() { };" << std::endl;
   fout << "inline void " << className << "::Clear() { };" << std::endl;
}

