/**
 * Thomas Keck 2015
 */

#include "FastBDT.h"
#include "FastBDT_IO.h"

extern "C" {

    void PrintVersion();

    struct Expertise {
      FastBDT::Forest<double> forest;
      unsigned int nBinningLevels;
      unsigned int nTrees;
      double shrinkage;
      double randRatio;
      bool transform2probability;
      unsigned int nLayersPerTree;
    };

    void* Create();

    void SetNBinningLevels(void *ptr, unsigned int nBinningLevels);
    
    void SetNTrees(void *ptr, unsigned int nTrees);
    
    void SetNLayersPerTree(void *ptr, unsigned int nLayersPerTree);
    
    void SetShrinkage(void *ptr, double shrinkage);
    
    void SetRandRatio(void *ptr, double randRatio);
    
    void SetTransform2Probability(void *ptr, bool transform2probability);

    void Delete(void *ptr);
    
    void Train(void *ptr, void *data_ptr, void *weight_ptr, void *target_ptr, unsigned int nEvents, unsigned int nFeatures);

    void Load(void* ptr, char *weightfile);

    double Analyse(void *ptr, double *array);

    void AnalyseArray(void *ptr, double *array, double *result, unsigned int nEvents, unsigned int nFeatures);

    void Save(void* ptr, char *weightfile);
    
    struct VariableRanking {
        std::map<unsigned int, double> ranking;
    }; 

    void* GetVariableRanking(void* ptr);
    
    unsigned int ExtractNumberOfVariablesFromVariableRanking(void* ptr);
    
    double ExtractImportanceOfVariableFromVariableRanking(void* ptr, unsigned int iFeature);
    
    void DeleteVariableRanking(void* ptr);

}
