// This program performs GP analysis (MLE, kriging) by using the RLCM
// method. It is a slight variant of GP_RLCM_NoNugget.cpp in that
// sampling is not done by itself but the data comes from the
// simulation of GP_Standard_NoNugget_Same_Field.cpp.
//
// The current implementation uses a d-dimensional grid (DPointArray),
// an isotropic Matern kernel, and 3 parameters for the kernel: alpha,
// ell, and nu. The actual global scaling is s = 10^alpha. There is no
// nugget.
//
// The current implementation supports parallelism. One may set the
// USE_OPENBLAS flag at compile-time so that all the BLAS and LAPACK
// routines are threaded. Additionally, one may set the USE_OPENMP
// flag so that other parts are threaded.
//
// Compile-time macros:
//
//   USE_OPENBLAS:     Either use this macro or not (no value)
//   USE_OPENMP:       Either use this macro or not (no value)
//
// Usage:
//
//   GP_RLCM_NoNugget_Same_Field.ex NumThreads Num_alpha List_alpha
//   Num_ell List_ell Num_nu List_nu r DiagCorrect Seed
//   IsCheckFiniteDiff DiffStepSize RandomFieldFileBasename
//   OutputLogLik [LogLikFileName] IsComputeFisher OutputFisher
//   [FisherFileName] OutputKrigedRandomField
//   [KrigedRandomFieldFileBasename] OutputPredictions
//   [PredictionsFileName]
//
//   NumThreads:   Number of threads if either USE_OPENBLAS or
//                 USE_OPENMP is set
//   Num_alpha:    (Param grid search) Length of list of alpha
//   List_alpha:   (Param grid search) List of alpha
//   Num_ell:      (Param grid search) Length of list of ell
//   List_ell:     (Param grid search) List of ell
//   Num_nu:       (Param grid search) Length of list of nu
//   List_nu:      (Param grid search) List of nu
//   r:            (Matrix structure) Rank
//   DiagCorrect:  (Matrix structure) Diagonal correction, e.g., 1e-8
//   Seed:         If >= 0, will use this value as the seed for RNG;
//                 otherwise, use the current time to seed the RNG.
//   IsCheckFiniteDiff:       If > 0, inspect what diff step size is appropriate
//   DiffStepSize:            An array of NUM_PARAM numbers
//   RandomFieldFileBasename: File basename (no ext) of data
//   OutputLogLik:            If > 0, output calculated loglik's to file
//   LogLikFileName:                If above > 0, file name (including ext)
//   IsComputeFisher:         If > 0, compute Fisher and related quantities
//   OutputFisher:            If > 0 and above > 0, output Fisher to file
//   FisherFileName:                If above two > 0, file name (including ext)
//   OutputKrigedRandomField: If > 0, output kriged random field to file
//   KrigedRandomFieldFileBasename: If above > 0, file basename (no ext)
//   OutputPredictions:       If > 0, output predictions to file
//   PredictionsFileName:           If above > 0, file name (including ext)
//
// For file formats of the output files, see GP_Common.hpp.

#include "GP_Common.hpp"

#define NUM_PARAM 3

int main(int argc, char **argv) {

  //---------- Parameters from command line --------------------

  INTEGER idx = 1;
  int NumThreads = atoi(argv[idx++]);

  // Param grid search
  INTEGER Num_alpha = String2Integer(argv[idx++]);
  double *List_alpha = NULL;
  New_1D_Array<double, INTEGER>(&List_alpha, Num_alpha);
  for (INTEGER i = 0; i < Num_alpha; i++) {
    List_alpha[i] = atof(argv[idx++]);
  }
  INTEGER Num_ell = String2Integer(argv[idx++]);
  double *List_ell = NULL;
  New_1D_Array<double, INTEGER>(&List_ell, Num_ell);
  for (INTEGER i = 0; i < Num_ell; i++) {
    List_ell[i] = atof(argv[idx++]);
  }
  INTEGER Num_nu = String2Integer(argv[idx++]);
  double *List_nu = NULL;
  New_1D_Array<double, INTEGER>(&List_nu, Num_nu);
  for (INTEGER i = 0; i < Num_nu; i++) {
    List_nu[i] = atof(argv[idx++]);
  }

  // Matrix structure
  INTEGER r = String2Integer(argv[idx++]);
  double mDiagCorrect = atof(argv[idx++]);

  // RNG
  INTEGER sSeed = String2Integer(argv[idx++]);
  unsigned Seed;
  if (sSeed < 0) {
    Seed = (unsigned)time(NULL);
  }
  else {
    Seed = (unsigned)sSeed;
  }

  // Finite difference
  bool IsCheckFiniteDiff = atoi(argv[idx++]) ? true : false;
  double DiffStepSize[NUM_PARAM];
  for (INTEGER i = 0; i < NUM_PARAM; i++) {
    DiffStepSize[i] = atof(argv[idx++]);
  }

  // Diagnostics
  char *RandomFieldFileBasename = argv[idx++];
  bool OutputLogLik = atoi(argv[idx++]) ? true : false;
  char *LogLikFileName = NULL;
  if (OutputLogLik) {
    LogLikFileName = argv[idx++];
  }
  bool IsComputeFisher = atoi(argv[idx++]) ? true : false;
  bool OutputFisher = atoi(argv[idx++]) ? true : false;
  char *FisherFileName = NULL;
  if (OutputFisher) {
    FisherFileName = argv[idx++];
  }
  bool OutputKrigedRandomField = atoi(argv[idx++]) ? true : false;
  char *KrigedRandomFieldFileBasename = NULL;
  if (OutputKrigedRandomField) {
    KrigedRandomFieldFileBasename = argv[idx++];
  }
  bool OutputPredictions = atoi(argv[idx++]) ? true : false;
  char *PredictionsFileName = NULL;
  if (OutputPredictions) {
    PredictionsFileName = argv[idx++];
  }

  //---------- Threading --------------------

#ifdef USE_OPENBLAS
  openblas_set_num_threads(NumThreads);
#elif defined USE_OPENMP
  omp_set_num_threads(NumThreads);
#else
  NumThreads = 1; // To avoid compiler warining of unused variable
#endif

  //---------- Main Computation --------------------

  // Timing
  PREPARE_CLOCK(true);

  // Seed the RNG
  srandom(Seed);

  // Load random field info
  // Grid
  INTEGER d = 0;
  INTEGER *Dim = NULL;
  INTEGER N = 0;
  double *Lower = NULL;
  double *Upper = NULL;
  double *Param = NULL;
  INTEGER NumParam = 0;
  ReadRandomFieldInfo(d, &Dim, N, &Lower, &Upper, NumParam, &Param,
                      RandomFieldFileBasename, "GP_RLCM_NoNugget_Same_Field");
  if (NumParam != NUM_PARAM) {
    printf("GP_RLCM_NoNugget_Same_Field: Error in number of parameters!\n");
    return 0;
  }
  double alpha = Param[0];
  double ell   = Param[1];
  double nu    = Param[2];

  // Load random field
  DVector y;
  ReadRandomField(y, RandomFieldFileBasename, "GP_RLCM_NoNugget_Same_Field");
  if (y.GetN() != N) {
    printf("GP_RLCM_NoNugget_Same_Field: Error in loading random field!\n");
    return 0;
  }

  // Load train/test split info
  INTEGER *IdxTrain = NULL, *IdxTest = NULL;
  INTEGER Ntrain = 0, Ntest = 0;
  ReadTrainTestSplit(&IdxTrain, &IdxTest, Ntrain, Ntest,
                     RandomFieldFileBasename, "GP_RLCM_NoNugget_Same_Field");
  if (Ntrain + Ntest != N) {
    printf("GP_RLCM_NoNugget_Same_Field: Error in loading train/test split!\n");
    return 0;
  }

  // Generate grid X
  DPointArray X;
  X.SetRegularGrid(d, Dim, Lower, Upper);

  // Perform train/test split
  DPointArray Xtrain, Xtest;
  DVector ytrain, ytest;
  X.GetSubset(IdxTrain, Ntrain, Xtrain);
  X.GetSubset(IdxTest, Ntest, Xtest);
  y.GetBlock(IdxTrain, Ntrain, ytrain);
  y.GetBlock(IdxTest, Ntest, ytest);

  // Save some memory
  X.ReleaseAllMemory();
  y.ReleaseAllMemory();

  // No permutation
  INTEGER *Perm = NULL;
  New_1D_Array<INTEGER, INTEGER>(&Perm, N);
  for (INTEGER i = 0; i < N; i++) {
    Perm[i] = i;
  }

  // DiagCorrect
  double lambda = mDiagCorrect;

  // The kernel matrix K(X,X) is not used anymore. In what follows, we
  // use Ktrain to denote the training kernel matrix K(Xtrain,
  // Xtrain). We need to rebuild a spatial partitioning because the
  // point set changes from X to Xtrain.
  CMatrix Ktrain;
  //INTEGER Ntrain = Xtrain.GetN();
  INTEGER *PermXtrain = NULL;
  New_1D_Array<INTEGER, INTEGER>(&PermXtrain, Ntrain);
  INTEGER NumLevel = (INTEGER)log2((double)Ntrain/r); // excluding root level
  Ktrain.BuildTree<DPoint, DPointArray>(Xtrain, PermXtrain, NULL, r,
                                        NumLevel, mDiagCorrect, Seed, BBOX);

  // Xtrain is permuted in Ktrain.BuildTree(). Accordingly permute
  // ytrain
  ytrain.Permute(PermXtrain, Ntrain);

  // MLE through grid search
  INTEGER ListLength = Num_alpha * Num_ell * Num_nu;
  double *mLogLik = NULL;
  New_1D_Array<double, INTEGER>(&mLogLik, ListLength);
  double **ListParam = NULL;
  New_2D_Array<double, INTEGER, INTEGER>(&ListParam, ListLength, NUM_PARAM);
  MLE_RLCM<IsotropicMatern, DPoint, DPointArray> mMLE;
  START_CLOCK;
  INTEGER t = 0;
  for (INTEGER i = 0; i < Num_alpha; i++) {
    double alpha = List_alpha[i];
    for (INTEGER j = 0; j < Num_ell; j++) {
      double ell = List_ell[j];
      for (INTEGER k = 0; k < Num_nu; k++) {
        double nu = List_nu[k];
        ListParam[t][0] = alpha;
        ListParam[t][1] = ell;
        ListParam[t][2] = nu;
        double s        = pow(10.0, alpha);
        IsotropicMatern mKernel(s, nu, ell);
        mLogLik[t] = mMLE.LogLik(Ktrain, Xtrain, ytrain, mKernel, lambda);
        printf("MLE_RLCM: Grid search alpha = %g, ell = %g, nu = %g, loglik = %.16e\n", alpha, ell, nu, mLogLik[t]); fflush(stdout);
        t++;
      }
    }
  }
  END_CLOCK;
  double TimeMLE = ELAPSED_TIME;

  double HatParam[NUM_PARAM];
  double MaxLogLik;
  EstimatedParam(NUM_PARAM, ListLength, ListParam, mLogLik,
                 HatParam, MaxLogLik);
  double hat_alpha = HatParam[0];
  double hat_ell   = HatParam[1];
  double hat_nu    = HatParam[2];
  printf("GP_RLCM_NoNugget_Same_Field: Truth     alpha = %g, ell = %g, nu = %g\n", alpha, ell, nu); fflush(stdout);
  printf("GP_RLCM_NoNugget_Same_Field: Estimated alpha = %g, ell = %g, nu = %g, max loglik = %.16e, MLE time = %gs\n", hat_alpha, hat_ell, hat_nu, MaxLogLik, TimeMLE); fflush(stdout);

  // Output all loglik's to file
  if (OutputLogLik) {
    WriteLogLikToFile(NUM_PARAM, ListLength, ListParam, mLogLik,
                      LogLikFileName, "GP_RLCM_NoNugget_Same_Field");
  }

  // Finite difference
  if (IsCheckFiniteDiff) {

    double delta = 1e-3;
    double fac = 2.0;
    INTEGER NumStep = 10;
    for (INTEGER i = 0; i < NUM_PARAM; i++) {
      printf("\nNumerical differentiation for param #%ld:\n", (long)i); fflush(stdout);
      printf("Step size                First difference          Second difference\n"); fflush(stdout);
      for (INTEGER j = 0; j < NumStep; j++) {

        double CenterParam[NUM_PARAM];
        memcpy(CenterParam, HatParam, NUM_PARAM*sizeof(double));
        double epsilon = delta / pow(fac, j);
        CenterParam[i] += epsilon;
        double alpha  = CenterParam[0];
        double ell    = CenterParam[1];
        double nu     = CenterParam[2];
        double s      = pow(10.0, alpha);
        IsotropicMatern mKernel(s, nu, ell);
        double mLogLikp = mMLE.LogLik(Ktrain, Xtrain, ytrain, mKernel, lambda);

        memcpy(CenterParam, HatParam, NUM_PARAM*sizeof(double));
        CenterParam[i] -= epsilon;
        alpha  = CenterParam[0];
        ell    = CenterParam[1];
        nu     = CenterParam[2];
        s      = pow(10.0, alpha);
        mKernel.Init(s, nu, ell);
        double mLogLikm = mMLE.LogLik(Ktrain, Xtrain, ytrain, mKernel, lambda);

        double FirstDiff = (mLogLikp - mLogLikm) / (2.0*epsilon);
        double SecondDiff = (mLogLikp + mLogLikm - 2.0*MaxLogLik) /
          (epsilon*epsilon);

        printf("%.16e   %+.16e   %+.16e\n", epsilon, FirstDiff, SecondDiff); fflush(stdout);
      }
    }

  }

  // Fisher information
  if (IsComputeFisher) {

    INTEGER ListLength2 = 2 * NUM_PARAM + 4 * NUM_PARAM*(NUM_PARAM-1)/2;
    double *mLogLik2 = NULL;
    New_1D_Array<double, INTEGER>(&mLogLik2, ListLength2);
    double **ListParam2 = NULL;
    New_2D_Array<double, INTEGER, INTEGER>(&ListParam2, ListLength2, NUM_PARAM);
    PrepareListParamForFisher(NUM_PARAM, ListLength2, ListParam2, HatParam,
                              DiffStepSize);

    for (INTEGER i = 0; i < ListLength2; i++) {
      double alpha = ListParam2[i][0];
      double ell   = ListParam2[i][1];
      double nu    = ListParam2[i][2];
      double s     = pow(10.0, alpha);
      IsotropicMatern mKernel(s, nu, ell);
      mLogLik2[i] = mMLE.LogLik(Ktrain, Xtrain, ytrain, mKernel, lambda);
      printf("MLE_RLCM: Fisher info alpha = %g, ell = %g, nu = %g, loglik = %.16e\n", alpha, ell, nu, mLogLik2[i]); fflush(stdout);
    }

    DMatrix Fisher, Cov;
    DVector Stderr;
    ComputeFisher(NUM_PARAM, ListLength2, ListParam2, mLogLik2,
                  HatParam, MaxLogLik, DiffStepSize, Fisher, Cov, Stderr);
    double *mStderr = Stderr.GetPointer();
    printf("GP_RLCM_NoNugget_Same_Field: Stderr alpha = %g, ell = %g, nu = %g\n", mStderr[0], mStderr[1], mStderr[2]); fflush(stdout);

    Delete_1D_Array<double>(&mLogLik2);
    Delete_2D_Array<double, INTEGER>(&ListParam2, ListLength2);

    // Output Fisher to file
    if (OutputFisher) {
      WriteFisherToFile(Fisher, FisherFileName, "GP_RLCM_NoNugget_Same_Field");
    }

  }

  // Use estimated parameters to do kriging
  Kriging_RLCM<IsotropicMatern, DPoint, DPointArray> mKriging;
  DVector ytest_predict, ytest_stddev;
  double hat_s = pow(10.0, hat_alpha);
  IsotropicMatern mKernel2(hat_s, hat_nu, hat_ell);
  START_CLOCK;
  mKriging.Train(Ktrain, Xtrain, mKernel2, lambda);
  mKriging.Test(Ktrain, Xtrain, Xtest, ytrain, mKernel2, lambda,
                ytest_predict, ytest_stddev);
  END_CLOCK;
  double TimeKriging = ELAPSED_TIME;
  printf("GP_RLCM_NoNugget_Same_Field: Kriging time = %gs\n", TimeKriging); fflush(stdout);

  // Output kriged random field to file
  if (OutputKrigedRandomField) {
    DVector ytrain_recover_order = ytrain;
    ytrain_recover_order.iPermute(PermXtrain, Ntrain);
    DVector y_all;
    AssembleY(ytrain_recover_order, ytest_predict, IdxTrain, IdxTest, y_all);
    DVector y_all_grid_order = y_all;
    y_all_grid_order.iPermute(Perm, N);
    WriteRandomFieldToFile(y_all_grid_order, d, Dim, Lower, Upper,
                           NUM_PARAM, HatParam,
                           KrigedRandomFieldFileBasename,
                           "GP_RLCM_NoNugget_Same_Field");
    ytrain_recover_order.ReleaseAllMemory();
    y_all.ReleaseAllMemory();
    y_all_grid_order.ReleaseAllMemory();
  }

  // Output predictions to file
  if (OutputPredictions) {
    WritePredictionsToFile(ytest, ytest_predict, ytest_stddev,
                           PredictionsFileName, "GP_RLCM_NoNugget_Same_Field");
  }

  //---------- Clean up --------------------

  Delete_1D_Array<INTEGER>(&Dim);
  Delete_1D_Array<double>(&Lower);
  Delete_1D_Array<double>(&Upper);
  Delete_1D_Array<double>(&Param);
  Delete_1D_Array<double>(&List_alpha);
  Delete_1D_Array<double>(&List_ell);
  Delete_1D_Array<double>(&List_nu);
  Delete_1D_Array<INTEGER>(&Perm);
  Delete_1D_Array<INTEGER>(&PermXtrain);
  Delete_1D_Array<INTEGER>(&IdxTrain);
  Delete_1D_Array<INTEGER>(&IdxTest);
  Delete_1D_Array<double>(&mLogLik);
  Delete_2D_Array<double, INTEGER>(&ListParam, ListLength);

  return 0;

}
