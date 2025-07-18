#include "ROOT/TestSupport.hxx"
#include "ROOT/RDataFrame.hxx"
#include "ROOT/RTrivialDS.hxx"
#include "ROOT/TSeq.hxx"
#include "Compression.h"
#include "TFile.h"
#include "TROOT.h"
#include "TSystem.h"
#include <TInterpreter.h>
#include "TTree.h"
#include "gtest/gtest.h"
#include <memory>
#include <thread>

#include <TTreeReader.h>
#include <TTreeReaderArray.h>

#include "DummyHeader.hxx"
#include "DataFrameSnapshotUtils.hxx"

using namespace ROOT;              // RDataFrame
using namespace ROOT::RDF;         // RInterface
using namespace ROOT::VecOps;      // RVec
using namespace ROOT::Detail::RDF; // RLoopManager

/********* FIXTURES *********/
// fixture that provides a RDF with no data-source and a single integer column "ans" with value 42
class RDFSnapshot : public ::testing::Test {
protected:
   const ULong64_t nEvents = 100ull; // must be initialized before fLoopManager

private:
   RDataFrame fTdf;
   RInterface<RLoopManager> DefineAns()
   {
      return fTdf.Define("ans", []() { return 42; });
   }

protected:
   RDFSnapshot() : fTdf(nEvents), tdf(DefineAns()) {}
   RInterface<RLoopManager> tdf;
};

#ifdef R__USE_IMT
struct TIMTEnabler {
   TIMTEnabler(unsigned int nSlots) { ROOT::EnableImplicitMT(nSlots); }
   ~TIMTEnabler() { ROOT::DisableImplicitMT(); }
};

// fixture that enables implicit MT and provides a RDF with no data-source and a single column "x" containing
// normal-distributed doubles
class RDFSnapshotMT : public ::testing::Test {
protected:
   const ULong64_t kNEvents = 100ull; // must be initialized before fLoopManager
   const unsigned int kNSlots = 4u;

private:
   TIMTEnabler fIMTEnabler;
   RDataFrame fTdf;
   RInterface<RLoopManager> DefineAns()
   {
      return fTdf.Define("ans", []() { return 42; });
   }

protected:
   RDFSnapshotMT() : fIMTEnabler(kNSlots), fTdf(kNEvents), tdf(DefineAns()) {}
   RInterface<RLoopManager> tdf;
};
#endif // R__USE_IMT

// Test for custom basket size in Snapshot
class SnapshotCustomBasketRAII {
private:
   std::string fInputFile;
   std::string fOutputFileCustom;
   std::string fOutputFileCollection;

public:
   SnapshotCustomBasketRAII()
      : fInputFile("input_file.root"),
        fOutputFileCustom("output_file_custom_basket.root"),
        fOutputFileCollection("output_file_collection_basket.root")
   {

      // Create file and tree inside the constructor body
      TFile fFile(fInputFile.c_str(), "RECREATE");
      TTree fTree("tree", "Test Tree");

      // Create scalar branch
      float value = 0;
      fTree.Branch("branch_x", &value);

      // Create vector branch
      std::vector<float> vec_values;
      fTree.Branch("branch_vec", &vec_values);

      // Fill the tree with some data
      for (int i = 0; i < 1000; ++i) {
         value = i;
         vec_values.clear();
         // Add some random number of elements to the vector
         int vec_size = i % 10 + 1; // 1 to 10 elements
         for (int j = 0; j < vec_size; ++j) {
            vec_values.push_back(i + j * 0.1f);
         }
         fTree.Fill();
      }
      fFile.Write();
   }

   ~SnapshotCustomBasketRAII()
   {
      gSystem->Unlink(fInputFile.c_str());
      gSystem->Unlink(fOutputFileCustom.c_str());
      gSystem->Unlink(fOutputFileCollection.c_str());
   }

   const std::string &GetInputFile() const { return fInputFile; }
   const std::string &GetOutputFileCustom() const { return fOutputFileCustom; }
   const std::string &GetOutputFileCollection() const { return fOutputFileCollection; }
};

void TestCustomBasketSize()
{
   SnapshotCustomBasketRAII raii;

   ROOT::RDataFrame df("tree", raii.GetInputFile());

   auto df_with_new_columns = df.Define("branch_x_new", [](float x) { return x * 2; }, {"branch_x"})
                                 .Define("branch_vec_new",
                                         [](const std::vector<float> &vec) {
                                            std::vector<float> result;
                                            result.reserve(vec.size());
                                            for (auto v : vec)
                                               result.push_back(v * 2);
                                            return result;
                                         },
                                         {"branch_vec"});

   ROOT::RDF::RSnapshotOptions options;
   options.fBasketSize = 2048;

   df_with_new_columns.Snapshot("tree", raii.GetOutputFileCustom(), {"branch_x", "branch_x_new"}, options);

   df_with_new_columns.Snapshot("tree", raii.GetOutputFileCollection(), {"branch_vec", "branch_vec_new"}, options);

   TFile output_file_custom(raii.GetOutputFileCustom().c_str());
   auto output_tree_custom = output_file_custom.Get<TTree>("tree");

   for (auto b : TRangeDynCast<TBranch>(*output_tree_custom->GetListOfBranches())) {
      ASSERT_TRUE(b != nullptr);
      EXPECT_EQ(b->GetBasketSize(), 2048) << "Incorrect basket size for scalar branch " << b->GetName();
   }

   TFile output_file_collection(raii.GetOutputFileCollection().c_str());
   auto output_tree_collection = output_file_collection.Get<TTree>("tree");

   for (auto b : TRangeDynCast<TBranch>(*output_tree_collection->GetListOfBranches())) {
      ASSERT_TRUE(b != nullptr);
      EXPECT_EQ(b->GetBasketSize(), 2048) << "Incorrect basket size for vector branch " << b->GetName();
   }
}

void TestDefaultBasketSize()
{
   SnapshotCustomBasketRAII helper;
   const Int_t defaultBasketSize = 32000;

   ROOT::RDataFrame df("tree", helper.GetInputFile());

   df.Snapshot("tree", helper.GetOutputFileCustom());

   std::unique_ptr<TFile> f(TFile::Open(helper.GetOutputFileCustom().c_str()));
   ASSERT_TRUE(f != nullptr);

   auto tree = f->Get<TTree>("tree");
   ASSERT_TRUE(tree != nullptr);

   auto branchX = tree->GetBranch("branch_x");
   ASSERT_TRUE(branchX != nullptr);
   EXPECT_EQ(branchX->GetBasketSize(), defaultBasketSize) << "Scalar branch doesn't have default basket size";

   auto branchVec = tree->GetBranch("branch_vec");
   ASSERT_TRUE(branchVec != nullptr);
   EXPECT_EQ(branchVec->GetBasketSize(), defaultBasketSize) << "Vector branch doesn't have default basket size";
}

void TestBasketSizePreservation()
{
   SnapshotCustomBasketRAII helper;

   // Define the columns we want to snapshot
   const std::vector<std::string> columns = {"branch_x", "branch_vec"};

   // First create a tree with custom basket size
   {
      ROOT::RDataFrame df("tree", helper.GetInputFile());
      ROOT::RDF::RSnapshotOptions options;
      options.fBasketSize = 64000; // 64KB
      df.Snapshot("tree", helper.GetOutputFileCustom(), columns, options);
   }

   // Now read that tree and create new snapshot without specifying basket size
   {
      ROOT::RDataFrame df("tree", helper.GetOutputFileCustom());
      df.Snapshot("tree", helper.GetOutputFileCollection(), columns);
   }

   // Open both files and compare basket sizes
   std::unique_ptr<TFile> f1(TFile::Open(helper.GetOutputFileCustom().c_str()));
   std::unique_ptr<TFile> f2(TFile::Open(helper.GetOutputFileCollection().c_str()));
   ASSERT_TRUE(f1 != nullptr);
   ASSERT_TRUE(f2 != nullptr);

   auto tree1 = f1->Get<TTree>("tree");
   auto tree2 = f2->Get<TTree>("tree");
   ASSERT_TRUE(tree1 != nullptr);
   ASSERT_TRUE(tree2 != nullptr);

   // Check both branches
   for (const auto &branchName : columns) {
      auto branch1 = tree1->GetBranch(branchName.c_str());
      auto branch2 = tree2->GetBranch(branchName.c_str());
      ASSERT_TRUE(branch1 != nullptr);
      ASSERT_TRUE(branch2 != nullptr);

      EXPECT_EQ(branch2->GetBasketSize(), branch1->GetBasketSize())
         << "Branch '" << branchName << "' basket size not preserved";
   }
}

// Test for custom basket size
TEST(RDFSnapshotMore, CustomBasketSize)
{
   TestCustomBasketSize();
}

// Test for default basket size
TEST(RDFSnapshotMore, DefaultBasketSize)
{
   TestDefaultBasketSize();
}

// Test for basket size preservation
TEST(RDFSnapshotMore, BasketSizePreservation)
{
   TestBasketSizePreservation();
}

// fixture that provides fixed and variable sized arrays as RDF columns
class RDFSnapshotArrays : public ::testing::Test {
protected:
   const static unsigned int kNEvents = 10u;
   static const std::vector<std::string> kFileNames;

   static void SetUpTestCase()
   {
      // write files containing c-arrays
      const auto eventsPerFile = kNEvents / kFileNames.size();
      auto curEvent = 0u;
      for (const auto &fname : kFileNames) {
         TFile f(fname.c_str(), "RECREATE");
         TTree t("arrayTree", "arrayTree");

         // doubles, floats
         const unsigned int fixedSize = 4u;
         float fixedSizeArr[fixedSize];
         t.Branch("fixedSizeArr", fixedSizeArr, ("fixedSizeArr[" + std::to_string(fixedSize) + "]/F").c_str());
         unsigned int size = 0u;
         t.Branch("size", &size);
         double *varSizeArr = new double[eventsPerFile * 100u];
         t.Branch("varSizeArr", varSizeArr, "varSizeArr[size]/D");

         // bools. std::vector<bool> makes bool treatment in RDF special
         bool fixedSizeBoolArr[fixedSize];
         t.Branch("fixedSizeBoolArr", fixedSizeBoolArr,
                  ("fixedSizeBoolArr[" + std::to_string(fixedSize) + "]/O").c_str());
         bool *varSizeBoolArr = new bool[eventsPerFile * 100u];
         t.Branch("varSizeBoolArr", varSizeBoolArr, "varSizeBoolArr[size]/O");

         // for each event, fill array elements
         for (auto i : ROOT::TSeqU(eventsPerFile)) {
            for (auto j : ROOT::TSeqU(4)) {
               fixedSizeArr[j] = curEvent * j;
               fixedSizeBoolArr[j] = j % 2 == 0;
            }
            size = (i + 1) * 100u;
            for (auto j : ROOT::TSeqU(size)) {
               varSizeArr[j] = curEvent * j;
               varSizeBoolArr[j] = j % 2 == 0;
            }
            t.Fill();
            ++curEvent;
         }
         t.Write();

         delete[] varSizeArr;
         delete[] varSizeBoolArr;
      }
   }

   static void TearDownTestCase()
   {
      for (const auto &fname : kFileNames)
         gSystem->Unlink(fname.c_str());
   }
};
const std::vector<std::string> RDFSnapshotArrays::kFileNames = {"test_snapshotarray1.root", "test_snapshotarray2.root"};

/********* SINGLE THREAD TESTS ***********/

TEST_F(RDFSnapshot, SnapshotCallAmbiguities)
{
   auto filename = "Snapshot_interface.root";

   tdf.Snapshot("t", filename, "an.*");
   tdf.Snapshot("t", filename, {"ans"});
   tdf.Snapshot("t", filename, {{"ans"}});

   gSystem->Unlink(filename);
}

// Test for ROOT-9210
TEST_F(RDFSnapshot, Snapshot_aliases)
{
   const auto alias0 = "myalias0";
   const auto alias1 = "myalias1";
   auto tdfa = tdf.Alias(alias0, "ans");
   auto tdfb = tdfa.Define("vec", [] { return RVec<int>{1, 2, 3}; }).Alias(alias1, "vec");
   testing::internal::CaptureStderr();
   auto snap = tdfb.Snapshot("mytree", "Snapshot_aliases.root", {alias0, alias1});
   std::string err = testing::internal::GetCapturedStderr();
   EXPECT_TRUE(err.empty()) << err;
   auto names = snap->GetColumnNames();
   EXPECT_EQ(2U, names.size());
   EXPECT_EQ(names, std::vector<std::string>({alias0, alias1}));

   auto takenCol = snap->Alias("a", alias0).Take<int>("a");
   for (auto i : takenCol) {
      EXPECT_EQ(42, i);
   }
}

// Test for ROOT-9122
TEST_F(RDFSnapshot, Snapshot_nocolumnmatch)
{
   const auto fname = "snapshotnocolumnmatch.root";
   RDataFrame d(1);
   auto op = [&]() { d.Snapshot("t", fname, "x"); };
   EXPECT_ANY_THROW(op());
   gSystem->Unlink(fname);
}

void TestSnapshotUpdate(RInterface<RLoopManager> &tdf, const std::string &outfile, const std::string &tree1,
                        const std::string &tree2, bool overwriteIfExists)
{
   // test snapshotting two trees to the same file opened in "UPDATE" mode
   auto df = tdf.Define("x", [] { return 10; });
   auto s1 = df.Snapshot(tree1, outfile, {"x"});

   auto c1 = s1->Count();
   auto mean1 = s1->Mean<int>("x");
   EXPECT_EQ(100ull, *c1);
   EXPECT_DOUBLE_EQ(10., *mean1);

   RSnapshotOptions opts;
   opts.fMode = "UPDATE";
   opts.fOverwriteIfExists = overwriteIfExists;
   auto s2 = ROOT::RDataFrame(50ull).Define("x", [] { return 10; }).Snapshot(tree2, outfile, {"x"}, opts);

   auto c2 = s2->Count();
   auto mean2 = s2->Mean<int>("x");
   EXPECT_EQ(50ull, *c2);
   EXPECT_DOUBLE_EQ(10., *mean2);

   // check that the output file contains both trees
   std::unique_ptr<TFile> f(TFile::Open(outfile.c_str()));
   EXPECT_NE(nullptr, f->Get<TTree>(tree1.c_str()));
   EXPECT_NE(nullptr, f->Get<TTree>(tree2.c_str()));

   // clean-up
   gSystem->Unlink(outfile.c_str());
}

TEST_F(RDFSnapshot, Snapshot_update_diff_treename)
{
   // test snapshotting two trees with different names
   TestSnapshotUpdate(tdf, "snap_update_difftreenames.root", "t1", "t2", false);
}

TEST_F(RDFSnapshot, Snapshot_update_same_treename)
{
   bool exceptionCaught = false;
   try {
      // test snapshotting two trees with same name
      TestSnapshotUpdate(tdf, "snap_update_sametreenames.root", "t", "t", false);
   } catch (const std::invalid_argument &e) {
      const std::string msg =
         "Snapshot: tree \"t\" already present in file \"snap_update_sametreenames.root\". If you want to delete the "
         "original tree and write another, please set RSnapshotOptions::fOverwriteIfExists to true.";
      EXPECT_EQ(e.what(), msg);
      exceptionCaught = true;
   }
   EXPECT_TRUE(exceptionCaught);
}

TEST_F(RDFSnapshot, Snapshot_update_overwrite)
{
   // test snapshotting two trees with different names
   TestSnapshotUpdate(tdf, "snap_update_overwrite.root", "t", "t", true);
}

void test_snapshot_options(RInterface<RLoopManager> &tdf)
{
   RSnapshotOptions opts;
   opts.fAutoFlush = 10;
   opts.fMode = "RECREATE";
   opts.fCompressionLevel = 6;

   const auto outfile = "snapshot_test_opts.root";
   using RCAlgo = ROOT::RCompressionSetting::EAlgorithm;
   for (auto algorithm : {RCAlgo::kZLIB, RCAlgo::kLZMA, RCAlgo::kLZ4, RCAlgo::kZSTD}) {
      opts.fCompressionAlgorithm = algorithm;

      auto s = tdf.Snapshot("t", outfile, {"ans"}, opts);

      auto c = s->Count();
      auto min = s->Min<int>("ans");
      auto max = s->Max<int>("ans");
      auto mean = s->Mean<int>("ans");
      EXPECT_EQ(100ull, *c);
      EXPECT_EQ(42, *min);
      EXPECT_EQ(42, *max);
      EXPECT_EQ(42, *mean);

      std::unique_ptr<TFile> f(TFile::Open("snapshot_test_opts.root"));

      EXPECT_EQ(algorithm, f->GetCompressionAlgorithm());
      EXPECT_EQ(6, f->GetCompressionLevel());
   }

   // clean-up
   gSystem->Unlink(outfile);
}

TEST_F(RDFSnapshot, Snapshot_action_with_options)
{
   test_snapshot_options(tdf);
}

void checkSnapshotArrayFile(RResultPtr<RInterface<RLoopManager>> &df, unsigned int kNEvents)
{
   // fixedSizeArr and varSizeArr are RResultPtr<vector<vector<T>>>
   auto fixedSizeArr = df->Take<RVec<float>>("fixedSizeArr");
   auto varSizeArr = df->Take<RVec<double>>("varSizeArr");
   auto fixedSizeBoolArr = df->Take<RVec<bool>>("fixedSizeBoolArr");
   auto varSizeBoolArr = df->Take<RVec<bool>>("varSizeBoolArr");
   auto size = df->Take<unsigned int>("size");

   // check contents of fixed sized arrays
   const auto nEvents = fixedSizeArr->size();
   const auto fixedSizeSize = fixedSizeArr->front().size();
   EXPECT_EQ(nEvents, kNEvents);
   EXPECT_EQ(fixedSizeSize, 4u);
   for (auto i = 0u; i < nEvents; ++i) {
      for (auto j = 0u; j < fixedSizeSize; ++j) {
         EXPECT_DOUBLE_EQ(fixedSizeArr->at(i).at(j), i * j);
         EXPECT_EQ(fixedSizeBoolArr->at(i).at(j), j % 2 == 0);
      }
   }

   // check contents of variable sized arrays
   for (auto i = 0u; i < nEvents; ++i) {
      const auto thisSize = size->at(i);
      const auto &dv = varSizeArr->at(i);
      const auto &bv = varSizeBoolArr->at(i);
      EXPECT_EQ(thisSize, dv.size());
      EXPECT_EQ(thisSize, bv.size());
      for (auto j = 0u; j < thisSize; ++j) {
         EXPECT_DOUBLE_EQ(dv[j], i * j);
         const bool value = bv[j];
         const bool expected = j % 2 == 0;
         EXPECT_EQ(value, expected);
      }
   }
}

TEST_F(RDFSnapshotArrays, SingleThread)
{
   RDataFrame tdf("arrayTree", kFileNames);
   // template Snapshot
   // "size" _must_ be listed before "varSizeArr"!
   auto dt = tdf.Snapshot("outTree", "test_snapshotRVecoutST.root",
                          {"fixedSizeArr", "size", "varSizeArr", "varSizeBoolArr", "fixedSizeBoolArr"});

   checkSnapshotArrayFile(dt, kNEvents);
}

TEST_F(RDFSnapshotArrays, SingleThreadJitted)
{
   RDataFrame tdf("arrayTree", kFileNames);
   // jitted Snapshot
   // "size" _must_ be listed before "varSizeArr"!
   auto dj = tdf.Snapshot("outTree", "test_snapshotRVecoutSTJitted.root",
                          {"fixedSizeArr", "size", "varSizeArr", "varSizeBoolArr", "fixedSizeBoolArr"});

   checkSnapshotArrayFile(dj, kNEvents);
}

TEST_F(RDFSnapshotArrays, RedefineArray)
{
   RDataFrame df("arrayTree", kFileNames);
   auto df2 = df.Redefine("fixedSizeArr", [] { return ROOT::RVecF{42.f, 42.f}; })
                 .Snapshot("t", "test_snapshotRVecRedefineArray.root", {"fixedSizeArr"});
   df2->Foreach(
      [](const ROOT::RVecF &v) {
         EXPECT_EQ(v.size(), 2u); // not 4 as it was in the original input
         EXPECT_TRUE(All(v == ROOT::RVecF{42.f, 42.f}));
      },
      {"fixedSizeArr"});

   gSystem->Unlink("test_snapshotRVecRedefineArray.root");
}

void WriteColsWithCustomTitles(const std::string &tname, const std::string &fname)
{
   int i;
   float f;
   int a[2];
   TFile file(fname.c_str(), "RECREATE");
   TTree t(tname.c_str(), tname.c_str());
   auto b = t.Branch("float", &f);
   b->SetTitle("custom title");
   b = t.Branch("i", &i);
   b->SetTitle("custom title");
   b = t.Branch("arrint", &a, "arrint[2]/I");
   b->SetTitle("custom title");
   b = t.Branch("vararrint", &a, "vararrint[i]/I");
   b->SetTitle("custom title");

   i = 1;
   a[0] = 42;
   a[1] = 84;
   f = 4.2;
   t.Fill();

   i = 2;
   f = 8.4;
   t.Fill();

   t.Write();
}

void CheckColsWithCustomTitles(unsigned long long int entry, int i, const RVec<int> &arrint, const RVec<int> &vararrint,
                               float f)
{
   if (entry == 0) {
      EXPECT_EQ(i, 1);
      EXPECT_EQ(arrint.size(), 2u);
      EXPECT_EQ(arrint[0], 42);
      EXPECT_EQ(arrint[1], 84);
      EXPECT_EQ(vararrint.size(), 1u);
      EXPECT_EQ(vararrint[0], 42);
      EXPECT_FLOAT_EQ(f, 4.2f);
   } else if (entry == 1) {
      EXPECT_EQ(i, 2);
      EXPECT_EQ(arrint.size(), 2u);
      EXPECT_EQ(arrint[0], 42);
      EXPECT_EQ(arrint[1], 84);
      EXPECT_EQ(vararrint.size(), 2u);
      EXPECT_EQ(vararrint[0], 42);
      EXPECT_EQ(vararrint[1], 84);
      EXPECT_FLOAT_EQ(f, 8.4f);
   } else
      throw std::runtime_error("tree has more entries than expected");
}

TEST(RDFSnapshotMore, ColsWithCustomTitles)
{
   const auto fname = "colswithcustomtitles.root";
   const auto tname = "t";

   // write test tree
   WriteColsWithCustomTitles(tname, fname);

   // read and write test tree with RDF
   RDataFrame d(tname, fname);
   const std::string prefix = "snapshotted_";
   auto res_tdf = d.Snapshot(tname, prefix + fname, {"i", "float", "arrint", "vararrint"});

   // check correct results have been written out
   res_tdf->Foreach(CheckColsWithCustomTitles, {"tdfentry_", "i", "arrint", "vararrint", "float"});

   // clean-up
   gSystem->Unlink(fname);
   gSystem->Unlink((prefix + fname).c_str());
}

TEST(RDFSnapshotMore, ReadWriteStdVec)
{
   // write a TFile containing a std::vector
   const auto fname = "readwritestdvec.root";
   const auto treename = "t";
   TFile f(fname, "RECREATE");
   TTree t(treename, treename);
   std::vector<int> v({42});
   std::vector<bool> vb({true, false, true}); // std::vector<bool> is special, not in a good way
   t.Branch("v", &v);
   t.Branch("vb", &vb);
   t.Fill();
   // as an extra test, make sure that the vector reallocates between first and second entry
   v = std::vector<int>(100000, 84);
   vb = std::vector<bool>(100000, true);
   t.Fill();
   t.Write();
   f.Close();

   auto outputChecker = [&treename](const char *filename) {
      // check snapshot output
      TFile f2(filename);
      TTreeReader r(treename, &f2);
      TTreeReaderArray<int> rv(r, "v");
      TTreeReaderArray<bool> rvb(r, "vb");
      r.Next();
      EXPECT_EQ(rv.GetSize(), 1u);
      EXPECT_EQ(rv[0], 42);
      EXPECT_EQ(rvb.GetSize(), 3u);
      EXPECT_TRUE(rvb[0]);
      EXPECT_FALSE(rvb[1]);
      EXPECT_TRUE(rvb[2]);
      r.Next();
      EXPECT_EQ(rv.GetSize(), 100000u);
      EXPECT_EQ(rvb.GetSize(), 100000u);
      for (auto e : rv)
         EXPECT_EQ(e, 84);
      for (auto e : rvb)
         EXPECT_TRUE(e);
   };

   // read and write using RDataFrame

   const auto outfname1 = "out_readwritestdvec1.root";
   RDataFrame(treename, fname).Snapshot(treename, outfname1, {"v", "vb"});
   outputChecker(outfname1);

   const auto outfname2 = "out_readwritestdvec2.root";
   RDataFrame(treename, fname).Snapshot(treename, outfname2);
   outputChecker(outfname2);

   const auto outfname3 = "out_readwritestdvec3.root";
   RDataFrame(treename, fname).Snapshot(treename, outfname3, {"v", "vb"});
   outputChecker(outfname3);

   gSystem->Unlink(fname);
   gSystem->Unlink(outfname1);
   gSystem->Unlink(outfname2);
   gSystem->Unlink(outfname3);
}

void ReadWriteCarray(const char *outFileNameBase)
{
   // write a TFile containing a arrays
   std::string outFileNameBaseStr = outFileNameBase;
   const auto fname = outFileNameBaseStr + ".root";
   const auto treename = "t";
   TFile f(fname.c_str(), "RECREATE");
   TTree t(treename, treename);
   const auto maxArraySize = 100000U;
   auto size = 0;
   int v[maxArraySize];
   bool vb[maxArraySize];
   long int vl[maxArraySize];
   t.Branch("size", &size, "size/I");
   t.Branch("v", v, "v[size]/I");
   t.Branch("vb", vb, "vb[size]/O");
   t.Branch("vl", vl, "vl[size]/G");

   // use 2**33 as a larger-than-int value on 64 bits, otherwise just something larger than short (2**30)
   static constexpr long int longintTestValue = sizeof(long int) == 8 ? 8589934592 : 1073741824;

   // Size 1
   size = 1;
   v[0] = 12;
   vb[0] = true;
   vl[0] = longintTestValue;
   t.Fill();

   // Size 0 (see ROOT-9860)
   size = 0;
   t.Fill();

   // Size 100k: this reallocates!
   size = maxArraySize;
   for (auto i : ROOT::TSeqU(size)) {
      v[i] = 84;
      vb[i] = true;
      vl[i] = 42;
   }
   t.Fill();

   // Size 3
   size = 3;
   v[0] = 42;
   v[1] = 43;
   v[2] = 44;
   vb[0] = true;
   vb[1] = false;
   vb[2] = true;
   vl[0] = -1;
   vl[1] = 0;
   vl[2] = 1;
   t.Fill();

   t.Write();
   f.Close();

   auto outputChecker = [&treename](const char *filename) {
      // check snapshot output
      TFile f2(filename);
      TTreeReader r(treename, &f2);
      TTreeReaderArray<int> rv(r, "v");
      TTreeReaderArray<bool> rvb(r, "vb");
      TTreeReaderArray<long int> rvl(r, "vl");

      // Size 1
      EXPECT_TRUE(r.Next());
      EXPECT_EQ(rv.GetSize(), 1u);
      EXPECT_EQ(rv[0], 12);
      EXPECT_EQ(rvb.GetSize(), 1u);
      EXPECT_TRUE(rvb[0]);
      EXPECT_EQ(rvl.GetSize(), 1u);
      EXPECT_EQ(rvl[0], longintTestValue);

      // Size 0
      EXPECT_TRUE(r.Next());
      EXPECT_EQ(rv.GetSize(), 0u);
      EXPECT_EQ(rvb.GetSize(), 0u);
      EXPECT_EQ(rvl.GetSize(), 0u);

      // Size 100k
      EXPECT_TRUE(r.Next());
      EXPECT_EQ(rv.GetSize(), 100000u);
      EXPECT_EQ(rvb.GetSize(), 100000u);
      for (auto e : rv)
         EXPECT_EQ(e, 84);
      for (auto e : rvb)
         EXPECT_TRUE(e);
      for (auto e : rvl)
         EXPECT_EQ(e, 42);

      // Size 3
      EXPECT_TRUE(r.Next());
      EXPECT_EQ(rv.GetSize(), 3u);
      EXPECT_EQ(rv[0], 42);
      EXPECT_EQ(rv[1], 43);
      EXPECT_EQ(rv[2], 44);
      EXPECT_EQ(rvb.GetSize(), 3u);
      EXPECT_TRUE(rvb[0]);
      EXPECT_FALSE(rvb[1]);
      EXPECT_TRUE(rvb[2]);
      EXPECT_EQ(rvl.GetSize(), 3u);
      EXPECT_EQ(rvl[0], -1);
      EXPECT_EQ(rvl[1], 0);
      EXPECT_EQ(rvl[2], 1);

      EXPECT_FALSE(r.Next());
   };

   // read and write using RDataFrame
   const auto outfname1 = outFileNameBaseStr + "_out1.root";
   RDataFrame(treename, fname).Snapshot(treename, outfname1);
   outputChecker(outfname1.c_str());

   const auto outfname2 = outFileNameBaseStr + "_out2.root";
   RDataFrame(treename, fname).Snapshot(treename, outfname2, {"size", "v", "vb", "vl"});
   outputChecker(outfname2.c_str());

   gSystem->Unlink(fname.c_str());
   gSystem->Unlink(outfname1.c_str());
   gSystem->Unlink(outfname2.c_str());
}

TEST(RDFSnapshotMore, ReadWriteCarray)
{
   ReadWriteCarray("ReadWriteCarray");
}

struct TwoInts {
   int a, b;
};

void WriteTreeWithLeaves(const std::string &treename, const std::string &fname)
{
   TFile f(fname.c_str(), "RECREATE");
   TTree t(treename.c_str(), treename.c_str());

   TwoInts ti{1, 2};
   t.Branch("v", &ti, "a/I:b/I");

   // TODO add checks for reading of multiple nested levels ("w.v.a")
   // when ROOT-9312 is solved and RDF supports "w.v.a" nested notation

   t.Fill();
   t.Write();
}

TEST(RDFSnapshotMore, ReadWriteNestedLeaves)
{
   const auto treename = "t";
   const auto fname = "readwritenestedleaves.root";
   WriteTreeWithLeaves(treename, fname);
   RDataFrame d(treename, fname);
   const auto outfname = "out_readwritenestedleaves.root";
   ROOT::RDF::RNode d2(d);
   {
      ROOT::TestSupport::CheckDiagsRAII diagRAII;
      diagRAII.requiredDiag(kInfo, "Snapshot", "Column v.a will be saved as v_a");
      diagRAII.requiredDiag(kInfo, "Snapshot", "Column v.b will be saved as v_b");
      d2 = *d.Snapshot(treename, outfname, {"v.a", "v.b"});
   }
   EXPECT_EQ(d2.GetColumnNames(), std::vector<std::string>({"v_a", "v_b"}));
   auto check_a_b = [](int a, int b) {
      EXPECT_EQ(a, 1);
      EXPECT_EQ(b, 2);
   };
   d2.Foreach(check_a_b, {"v_a", "v_b"});
   gSystem->Unlink(fname);
   gSystem->Unlink(outfname);

   try {
      d.Define("v_a", [] { return 0; }).Snapshot(treename, outfname, {"v.a", "v_a"});
   } catch (std::runtime_error &e) {
      const auto error_msg = "Column v.a would be written as v_a but this column already exists. Please use Alias to "
                             "select a new name for v.a";
      EXPECT_STREQ(e.what(), error_msg);
   }
}

TEST(RDFSnapshotMore, Lazy)
{
   const auto treename = "t";
   const auto fname0 = "lazy0.root";
   const auto fname1 = "lazy1.root";
   // make sure the file is not here beforehand
   gSystem->Unlink(fname0);
   gSystem->Unlink(fname1);
   RDataFrame d(1);
   auto v = 0U;
   auto genf = [&v]() {
      ++v;
      return 42;
   };
   RSnapshotOptions opts = {"RECREATE", ROOT::RCompressionSetting::EAlgorithm::kZLIB, 0, 0, 99, true};
   auto ds = d.Define("c0", genf).Snapshot(treename, fname0, {"c0"}, opts);
   EXPECT_EQ(v, 0U);
   EXPECT_TRUE(gSystem->AccessPathName(fname0)); // This returns FALSE if the file IS there
   auto ds2 = ds->Define("c1", genf).Snapshot(treename, fname1, {"c1"}, opts);
   EXPECT_EQ(v, 1U);
   EXPECT_FALSE(gSystem->AccessPathName(fname0));
   EXPECT_TRUE(gSystem->AccessPathName(fname1));
   *ds2;
   EXPECT_EQ(v, 2U);
   EXPECT_FALSE(gSystem->AccessPathName(fname1));
   gSystem->Unlink(fname0);
   gSystem->Unlink(fname1);
}

TEST(RDFSnapshotMore, LazyJitted)
{
   const auto treename = "t";
   const auto fname = "lazyjittedsnapshot.root";
   // make sure the file is not here beforehand
   gSystem->Unlink(fname);
   RDataFrame d(1);
   RSnapshotOptions opts = {"RECREATE", ROOT::RCompressionSetting::EAlgorithm::kZLIB, 0, 0, 99, true};
   auto ds = d.Alias("c0", "rdfentry_").Snapshot(treename, fname, {"c0"}, opts);
   EXPECT_TRUE(gSystem->AccessPathName(fname)); // This returns FALSE if the file IS there
   *ds;
   EXPECT_FALSE(gSystem->AccessPathName(fname));
   gSystem->Unlink(fname);
}

void BookLazySnapshot()
{
   auto d = ROOT::RDataFrame(1);
   ROOT::RDF::RSnapshotOptions opts;
   opts.fLazy = true;
   d.Snapshot("t", "lazysnapshotnottriggered_shouldnotbecreated.root", {"rdfentry_"}, opts);
}

TEST(RDFSnapshotMore, LazyNotTriggered)
{
   ROOT_EXPECT_WARNING(BookLazySnapshot(), "Snapshot",
                       "A lazy Snapshot action was booked but never triggered. The tree 't' in output file "
                       "'lazysnapshotnottriggered_shouldnotbecreated.root' was not created. "
                       "In case it was desired instead, remember to trigger the Snapshot operation, by "
                       "storing its result in a variable and for example calling the GetValue() method on it.");
}

RResultPtr<RInterface<RLoopManager, void>> ReturnLazySnapshot(const char *fname)
{
   auto d = ROOT::RDataFrame(1);
   ROOT::RDF::RSnapshotOptions opts;
   opts.fLazy = true;
   auto res = d.Snapshot("t", fname, {"rdfentry_"}, opts);
   RResultPtr<RInterface<RLoopManager, void>> res2 = res;
   return res;
}

TEST(RDFSnapshotMore, LazyTriggeredAfterCopy)
{
   const auto fname = "lazysnapshottriggeredaftercopy.root";
   ROOT_EXPECT_NODIAG(*ReturnLazySnapshot(fname));
   gSystem->Unlink(fname);
}

void CheckTClonesArrayOutput(const RVec<TH1D> &hvec)
{
   ASSERT_EQ(hvec.size(), 3);
   for (int i = 0; i < 3; ++i) {
      EXPECT_EQ(hvec[i].GetEntries(), 1);
      EXPECT_DOUBLE_EQ(hvec[i].GetMean(), i);
   }
}

void ReadWriteTClonesArray()
{
   {
      TClonesArray arr("TH1D", 3);
      for (int i = 0; i < 3; ++i) {
         auto *h = static_cast<TH1D *>(arr.ConstructedAt(i));
         h->SetBins(25, 0, 10);
         h->Fill(i);
      }
      TFile f("df_readwritetclonesarray.root", "recreate");
      TTree t("t", "t");
      t.Branch("arr", &arr);
      t.Fill();
      t.Write();
      f.Close();
   }

   {
      // write as TClonesArray
      auto out_df =
         ROOT::RDataFrame("t", "df_readwritetclonesarray.root").Snapshot("t", "df_readwriteclonesarray1.root", {"arr"});
      RVec<TH1D> hvec;

#ifndef NDEBUG
      ROOT_EXPECT_WARNING(
         hvec = out_df->Take<RVec<TH1D>>("arr")->at(0), "RTreeColumnReader::Get",
         "Branch arr hangs from a non-split branch. A copy is being performed in order to properly read the content.");
#else
      ROOT_EXPECT_NODIAG(hvec = out_df->Take<RVec<TH1D>>("arr")->at(0));
#endif
      CheckTClonesArrayOutput(hvec);
   }

   // FIXME uncomment when ROOT-10801 is solved
   //{
   //   gInterpreter->GenerateDictionary("vector<TH1D,ROOT::Detail::VecOps::RAdoptAllocator<TH1D>>",
   //                                    "vector;TH1D.h;ROOT/RVec.hxx");
   //   // write as RVecs
   //   auto out_df = ROOT::RDataFrame("t", "df_readwritetclonesarray.root")
   //                    .Snapshot("t", "df_readwriteclonesarray2.root", {"arr"});
   //   const auto hvec = out_df->Take<RVec<TH1D>>("arr")->at(0);
   //   CheckTClonesArrayOutput(hvec);
   //}

   {
      // write as Snapshot wants
      auto out_df =
         ROOT::RDataFrame("t", "df_readwritetclonesarray.root").Snapshot("t", "df_readwriteclonesarray3.root", {"arr"});
      RVec<TH1D> hvec;
#ifndef NDEBUG
      ROOT_EXPECT_WARNING(
         hvec = out_df->Take<RVec<TH1D>>("arr")->at(0), "RTreeColumnReader::Get",
         "Branch arr hangs from a non-split branch. A copy is being performed in order to properly read the content.");
#else
      ROOT_EXPECT_NODIAG(hvec = out_df->Take<RVec<TH1D>>("arr")->at(0));
#endif
      CheckTClonesArrayOutput(hvec);
   }

   gSystem->Unlink("df_readwritetclonesarray.root");
   gSystem->Unlink("df_readwriteclonesarray1.root");
   gSystem->Unlink("df_readwriteclonesarray2.root");
   gSystem->Unlink("df_readwriteclonesarray3.root");
}

TEST(RDFSnapshotMore, TClonesArray)
{
   ReadWriteTClonesArray();
}

// https://its.cern.ch/jira/browse/ROOT-10702
TEST(RDFSnapshotMore, CompositeTypeWithNameClash)
{
   constexpr auto fName{"snap_compositetypewithnameclash.root"};
   struct FileGuard {
      ~FileGuard() { std::remove("snap_compositetypewithnameclash.root"); }
   } _;
   ROOT::RDataFrame df{3};
   auto snap_df = df.Define("i", [] { return Int{-1}; }).Define("x", [] { return 1; }).Snapshot("t", fName);
   EXPECT_EQ(snap_df->Sum<int>("x").GetValue(), 3); // prints -3 if the wrong "x" is written out
   EXPECT_EQ(snap_df->Sum<int>("i.x").GetValue(), -3);
}

// Test that we error out gracefully in case the output file specified for a Snapshot cannot be opened
TEST(RDFSnapshotMore, ForbiddenOutputFilename)
{
   ROOT::RDataFrame df(4);
   const auto out_fname = "/definitely/not/a/valid/path/f.root";

   // Compiled
   try {
      ROOT_EXPECT_SYSERROR(df.Snapshot("t", out_fname, {"rdfslot_"}), "TFile::TFile",
                           "file /definitely/not/a/valid/path/f.root can not be opened No such file or directory")
   } catch (const std::runtime_error &e) {
      EXPECT_STREQ(e.what(), "Snapshot: could not create output file /definitely/not/a/valid/path/f.root");
   }

   // Jitted
   // If some other test case called EnableThreadSafety, the error printed here is of the form
   // "SysError in <TFile::TFile>: file /definitely/not/a/valid/path/f.root can not be opened No such file or
   // directory\nError in <TReentrantRWLock::WriteUnLock>: Write lock already released for 0x55f179989378\n" but the
   // address printed changes every time
   ROOT::TestSupport::CheckDiagsRAII diagRAII{
      kSysError, "TFile::TFile",
      "file /definitely/not/a/valid/path/f.root can not be opened No such file or directory"};
   EXPECT_THROW(df.Snapshot("t", out_fname, {"rdfslot_"}), std::runtime_error);
}

TEST(RDFSnapshotMore, ZeroOutputEntries)
{
   const auto fname = "snapshot_zerooutputentries.root";
   ROOT::RDataFrame(10).Alias("c", "rdfentry_").Filter([] { return false; }).Snapshot("t", fname, {"c"});
   EXPECT_EQ(gSystem->AccessPathName(fname), 0); // This returns 0 if the file IS there

   TFile f(fname);
   auto *t = f.Get<TTree>("t");
   EXPECT_NE(t, nullptr);           // TTree "t" should be in there...
   EXPECT_EQ(t->GetEntries(), 0ll); // ...and have zero entries
   gSystem->Unlink(fname);
}

// Test for https://github.com/root-project/root/issues/10233
TEST(RDFSnapshotMore, RedefinedDSColumn)
{
   const auto fname = "test_snapshot_redefinedscolumn.root";
   auto df = ROOT::RDF::MakeTrivialDataFrame(1);

   df.Redefine("col0", [] { return 42; }).Snapshot("t", fname);
   gSystem->Unlink(fname);
}

// https://github.com/root-project/root/issues/6932
TEST(RDFSnapshotMore, MissingSizeBranch)
{
   const auto inFile = "test_snapshot_missingsizebranch.root";
   const auto outFile = "test_snapshot_missingsizebranch_out.root";

   // make input tree
   {
      TFile f(inFile, "recreate");
      TTree t("t", "t");
      int sz = 1;
      t.Branch("sz", &sz);
      float vec[3] = {1, 2, 3};
      t.Branch("vec", vec, "vec[sz]/F");
      t.Fill();
      sz = 2;
      t.Fill();
      sz = 3;
      t.Fill();
      t.Write();
   }

   ROOT::RDataFrame df("t", inFile);

   auto out = df.Snapshot("t", outFile, {"vec"});

   auto sizes = out->Take<int>("sz");
   auto vecs = out->Take<ROOT::RVecF>("vec");

   EXPECT_EQ(sizes->at(0), 1);
   EXPECT_EQ(sizes->at(1), 2);
   EXPECT_EQ(sizes->at(2), 3);
   EXPECT_TRUE(All(vecs->at(0) == ROOT::RVecF{1}));
   EXPECT_TRUE(All(vecs->at(1) == ROOT::RVecF{1, 2}));
   EXPECT_TRUE(All(vecs->at(2) == ROOT::RVecF{1, 2, 3}));

   gSystem->Unlink(inFile);
   gSystem->Unlink(outFile);
}

TEST(RDFSnapshotMore, OutOfOrderSizeBranch)
{
   const auto inFile = "test_snapshot_outofordersizebranch_in.root";
   const auto outFile = "test_snapshot_outofordersizebranch_out.root";

   // make input tree
   {
      TFile f(inFile, "recreate");
      TTree t("t", "t");
      int sz = 1;
      t.Branch("sz", &sz);
      float vec[3] = {1, 2, 3};
      t.Branch("vec", vec, "vec[sz]/F");
      t.Fill();
      sz = 2;
      t.Fill();
      sz = 3;
      t.Fill();
      t.Write();
   }

   auto check = [](const std::vector<int> &sizes, const std::vector<ROOT::RVecF> &vecs) {
      EXPECT_EQ(sizes.at(0), 1);
      EXPECT_EQ(sizes.at(1), 2);
      EXPECT_EQ(sizes.at(2), 3);
      EXPECT_TRUE(All(vecs.at(0) == ROOT::RVecF{1}));
      EXPECT_TRUE(All(vecs.at(1) == ROOT::RVecF{1, 2}));
      EXPECT_TRUE(All(vecs.at(2) == ROOT::RVecF{1, 2, 3}));
   };

   {
      // fully typed Snapshot
      auto out = ROOT::RDataFrame("t", inFile).Snapshot("t", outFile, {"vec", "sz"});
      auto sizes = out->Take<int>("sz");
      auto vecs = out->Take<ROOT::RVecF>("vec");

      check(*sizes, *vecs);
   }

   {
      // jitted Snapshot
      auto out = ROOT::RDataFrame("t", inFile).Snapshot("t", outFile, {"vec", "sz"});
      auto sizes = out->Take<int>("sz");
      auto vecs = out->Take<ROOT::RVecF>("vec");

      check(*sizes, *vecs);
   }

   gSystem->Unlink(inFile);
   gSystem->Unlink(outFile);
}

TEST(RDFSnapshotMore, PreserveStdVectorWithOptions)
{
   struct DatasetGuard {

      const char *fFileName{"rdfsnapshotmore_preservestdvectorwithoptions.root"};
      const char *fTreeName{"rdfsnapshotmore_preservestdvectorwithoptions"};
      const char *fSnapFileDefault{"rdfsnapshotmore_preservestdvectorwithoptions_snap_default.root"};
      const char *fSnapTreeDefault{"rdfsnapshotmore_preservestdvectorwithoptions_snap_default"};
      const char *fSnapFileOpts{"rdfsnapshotmore_preservestdvectorwithoptions_snap_opts.root"};
      const char *fSnapTreeOpts{"rdfsnapshotmore_preservestdvectorwithoptions_snap_opts"};

      DatasetGuard()
      {
         std::unique_ptr<TFile> f{TFile::Open(fFileName, "RECREATE")};
         auto t = std::make_unique<TTree>(fTreeName, fTreeName);
         std::vector<int> a{11, 22, 33};
         std::vector<float> b{44.f, 55.f, 66.f};
         std::vector<double> c{77., 88., 99.};
         t->Branch("a", &a);
         t->Branch("b", &b);
         t->Branch("c", &c);
         t->Fill();
         t->Write();
      }

      ~DatasetGuard()
      {
         std::remove(fFileName);
         std::remove(fSnapFileDefault);
         std::remove(fSnapFileOpts);
      }
   } dataset;

   // Snapshot with default options: std::vector branches are converted to ROOT::RVec
   {
      ROOT::RDataFrame df{dataset.fTreeName, dataset.fFileName};
      df.Snapshot(dataset.fSnapTreeDefault, dataset.fSnapFileDefault);
   }

   {
      std::unique_ptr<TFile> f{TFile::Open(dataset.fSnapFileDefault)};
      std::unique_ptr<TTree> t{f->Get<TTree>(dataset.fSnapTreeDefault)};
      std::vector<std::string> expected_typenames_default{"ROOT::VecOps::RVec<int>", "ROOT::VecOps::RVec<float>",
                                                          "ROOT::VecOps::RVec<double>"};
      std::vector<std::string> typenames_default;
      for (const auto *leaf : ROOT::Detail::TRangeStaticCast<TLeaf>(t->GetListOfLeaves())) {
         typenames_default.push_back(leaf->GetTypeName());
      }
      EXPECT_EQ(expected_typenames_default.size(), typenames_default.size());
      for (std::size_t i = 0; i < expected_typenames_default.size(); i++) {
         EXPECT_EQ(expected_typenames_default[i], typenames_default[i]);
      }
   }

   // Pass option to Snapshot to avoid the type conversion
   {
      ROOT::RDF::RSnapshotOptions opts;
      opts.fVector2RVec = false;
      ROOT::RDataFrame df{dataset.fTreeName, dataset.fFileName};
      df.Snapshot(dataset.fSnapTreeOpts, dataset.fSnapFileOpts, df.GetColumnNames(), opts);
   }

   {
      std::unique_ptr<TFile> f{TFile::Open(dataset.fSnapFileOpts)};
      std::unique_ptr<TTree> t{f->Get<TTree>(dataset.fSnapTreeOpts)};
      std::vector<std::string> expected_typenames_default{"vector<int>", "vector<float>", "vector<double>"};
      std::vector<std::string> typenames_default;
      for (const auto *leaf : ROOT::Detail::TRangeStaticCast<TLeaf>(t->GetListOfLeaves())) {
         typenames_default.push_back(leaf->GetTypeName());
      }
      EXPECT_EQ(expected_typenames_default.size(), typenames_default.size());
      for (std::size_t i = 0; i < expected_typenames_default.size(); i++) {
         EXPECT_EQ(expected_typenames_default[i], typenames_default[i]);
      }
   }
}

/********* MULTI THREAD TESTS ***********/
#ifdef R__USE_IMT
TEST_F(RDFSnapshotMT, Snapshot_update_diff_treename)
{
   // test snapshotting two trees with different names
   TestSnapshotUpdate(tdf, "snap_update_difftreenames.root", "t1", "t2", false);
}

TEST_F(RDFSnapshotMT, Snapshot_update_same_treename)
{
   bool exceptionCaught = false;
   try {
      // test snapshotting two trees with same name
      TestSnapshotUpdate(tdf, "snap_update_sametreenames.root", "t", "t", false);
   } catch (const std::invalid_argument &e) {
      const std::string msg =
         "Snapshot: tree \"t\" already present in file \"snap_update_sametreenames.root\". If you want to delete the "
         "original tree and write another, please set RSnapshotOptions::fOverwriteIfExists to true.";
      EXPECT_EQ(e.what(), msg);
      exceptionCaught = true;
   }
   EXPECT_TRUE(exceptionCaught);
}

TEST_F(RDFSnapshotMT, Snapshot_update_overwrite)
{
   // test snapshotting two trees with different names
   TestSnapshotUpdate(tdf, "snap_update_overwrite.root", "t", "t", true);
}

TEST_F(RDFSnapshotMT, Snapshot_action_with_options)
{
   test_snapshot_options(tdf);
}

TEST_F(RDFSnapshotMT, Reshuffled_friends)
{
   const auto fname = "snapshot_reshuffled_friends.root";
   tdf.Snapshot("t", fname);

   {
      // add reshuffled tree as friend
      TFile f(fname);
      TTree *t = f.Get<TTree>("t");
      TTree t2("t2", "t2");
      const auto expected =
         "Tree 't' has the kEntriesReshuffled bit set and cannot have friends nor can be added as a friend unless the "
         "main tree has a TTreeIndex on the friend tree 't'. You can also unset the bit manually if you know what you "
         "are doing; note that you risk associating wrong TTree entries of the friend with those of the main TTree!";
      ROOT_EXPECT_ERROR(t2.AddFriend(t), "AddFriend", expected);
   }

   {
      // add friend to reshuffled tree
      TFile f(fname);
      TTree *t = f.Get<TTree>("t");
      TTree t2("t2", "t2");
      const auto expected =
         "Tree 't' has the kEntriesReshuffled bit set and cannot have friends nor can be added as a friend unless the "
         "main tree has a TTreeIndex on the friend tree 't2'. You can also unset the bit manually if you know what you "
         "are doing; note that you risk associating wrong TTree entries of the friend with those of the main TTree!";
      ROOT_EXPECT_ERROR(t->AddFriend(&t2);, "AddFriend", expected);
   }
}

TEST(RDFSnapshotMore, ManyTasksPerThread)
{
   const auto nSlots = 4u;
   ROOT::EnableImplicitMT(nSlots);
   // make sure the file is not here beforehand
   gSystem->Unlink("snapshot_manytasks_out.root");

   // easiest way to be sure reading requires spawning of several tasks: create several input files
   const std::string inputFilePrefix = "snapshot_manytasks_";
   const auto tasksPerThread = 8u;
   const auto nInputFiles = nSlots * tasksPerThread;
   ROOT::RDataFrame d(1);
   auto dd = d.Define("x", []() { return 42; });
   for (auto i = 0u; i < nInputFiles; ++i)
      dd.Snapshot("t", inputFilePrefix + std::to_string(i) + ".root", {"x"});

   // test multi-thread Snapshotting from many tasks per worker thread
   const auto outputFile = "snapshot_manytasks_out.root";
   ROOT::RDataFrame tdf("t", inputFilePrefix + "*.root");
   tdf.Snapshot("t", outputFile, {"x"});

   // check output contents
   ROOT::RDataFrame checkTdf("t", outputFile);
   auto c = checkTdf.Count();
   auto t = checkTdf.Take<int>("x");
   for (auto v : t)
      EXPECT_EQ(v, 42);
   EXPECT_EQ(*c, nInputFiles);

   // clean-up input files
   for (auto i = 0u; i < nInputFiles; ++i)
      gSystem->Unlink((inputFilePrefix + std::to_string(i) + ".root").c_str());
   gSystem->Unlink(outputFile);

   ROOT::DisableImplicitMT();
}

void checkSnapshotArrayFileMT(RResultPtr<RInterface<RLoopManager>> &df, unsigned int kNEvents)
{
   // fixedSizeArr and varSizeArr are RResultPtr<vector<vector<T>>>
   auto fixedSizeArr = df->Take<RVec<float>>("fixedSizeArr");
   auto varSizeArr = df->Take<RVec<double>>("varSizeArr");
   auto size = df->Take<unsigned int>("size");

   // multi-thread execution might have scrambled events w.r.t. the original file, so we just check overall properties
   const auto nEvents = fixedSizeArr->size();
   EXPECT_EQ(nEvents, kNEvents);
   // TODO check more!
}

TEST_F(RDFSnapshotArrays, MultiThread)
{
   ROOT::EnableImplicitMT(4);

   RDataFrame tdf("arrayTree", kFileNames);
   auto dt = tdf.Snapshot("outTree", "test_snapshotRVecoutMT.root",
                          {"fixedSizeArr", "size", "varSizeArr", "varSizeBoolArr", "fixedSizeBoolArr"});

   checkSnapshotArrayFileMT(dt, kNEvents);

   ROOT::DisableImplicitMT();
}

TEST_F(RDFSnapshotArrays, MultiThreadJitted)
{
   ROOT::EnableImplicitMT(4);

   RDataFrame tdf("arrayTree", kFileNames);
   auto dj = tdf.Snapshot("outTree", "test_snapshotRVecoutMTJitted.root",
                          {"fixedSizeArr", "size", "varSizeArr", "varSizeBoolArr", "fixedSizeBoolArr"});

   checkSnapshotArrayFileMT(dj, kNEvents);

   ROOT::DisableImplicitMT();
}

// See also https://github.com/root-project/root/issues/10225
TEST_F(RDFSnapshotArrays, WriteRVecFromFile)
{
   {
      auto df = ROOT::RDataFrame(3).Define("x", [](ULong64_t e) { return ROOT::RVecD(e, double(e)); }, {"rdfentry_"});
      df.Snapshot("t", "test_snapshotRVecWriteRVecFromFile.root", {"x"});
   }

   ROOT::RDataFrame df("t", "test_snapshotRVecWriteRVecFromFile.root");
   auto outdf = df.Snapshot("t", "test_snapshotRVecWriteRVecFromFile2.root", {"x"});

   const auto res = outdf->Take<ROOT::RVecD>("x").GetValue();

   EXPECT_EQ(res.size(), 3u);
   EXPECT_EQ(res[0].size(), 0u);
   EXPECT_TRUE(All(res[1] == ROOT::RVecD{1.}));
   EXPECT_TRUE(All(res[2] == ROOT::RVecD{2., 2.}));

   gSystem->Unlink("test_snapshotRVecWriteRVecFromFile.root");
   gSystem->Unlink("test_snapshotRVecWriteRVecFromFile2.root");
}

TEST(RDFSnapshotMore, ColsWithCustomTitlesMT)
{
   const auto fname = "colswithcustomtitlesmt.root";
   const auto tname = "t";

   // write test tree
   WriteColsWithCustomTitles(tname, fname);

   // read and write test tree with RDF (in parallel)
   ROOT::EnableImplicitMT(4);
   RDataFrame d(tname, fname);
   const std::string prefix = "snapshotted_";
   auto res_tdf = d.Snapshot(tname, prefix + fname, {"i", "float", "arrint", "vararrint"});

   // check correct results have been written out
   res_tdf->Foreach(CheckColsWithCustomTitles, {"tdfentry_", "i", "arrint", "vararrint", "float"});
   res_tdf->Foreach(CheckColsWithCustomTitles, {"rdfentry_", "i", "arrint", "vararrint", "float"});

   // clean-up
   gSystem->Unlink(fname);
   gSystem->Unlink((prefix + fname).c_str());
   ROOT::DisableImplicitMT();
}

TEST(RDFSnapshotMore, TreeWithFriendsMT)
{
   const auto fname1 = "treewithfriendsmt1.root";
   const auto fname2 = "treewithfriendsmt2.root";
   RDataFrame(10).Define("x", []() { return 42; }).Snapshot("t", fname1, {"x"});
   RDataFrame(10).Define("x", []() { return 0; }).Snapshot("t", fname2, {"x"});

   ROOT::EnableImplicitMT();

   TFile file(fname1);
   auto tree = file.Get<TTree>("t");
   TFile file2(fname2);
   auto tree2 = file2.Get<TTree>("t");
   tree->AddFriend(tree2);

   const auto outfname = "out_treewithfriendsmt.root";
   RDataFrame df(*tree);
   auto df_out = df.Snapshot("t", outfname, {"x"});
   EXPECT_EQ(df_out->Max<int>("x").GetValue(), 42);
   EXPECT_EQ(df_out->GetColumnNames(), std::vector<std::string>{"x"});

   ROOT::DisableImplicitMT();
   gSystem->Unlink(fname1);
   gSystem->Unlink(fname2);
   gSystem->Unlink(outfname);
}

TEST(RDFSnapshotMore, JittedSnapshotAndAliasedColumns)
{
   ROOT::RDataFrame df(1);
   const auto fname = "out_aliaseddefine.root";
   // aliasing a custom column
   auto df2 = df.Define("x", [] { return 42; }).Alias("y", "x").Snapshot("t", fname, "y"); // must be jitted!
   EXPECT_EQ(df2->GetColumnNames(), std::vector<std::string>({"y"}));
   EXPECT_EQ(df2->Take<int>("y")->at(0), 42);

   // aliasing a column from a file
   const auto fname2 = "out_aliaseddefine2.root";
   auto df3 = df2->Alias("z", "y").Snapshot("t", fname2, "z");
   EXPECT_EQ(df3->GetColumnNames(), std::vector<std::string>({"z"}));
   EXPECT_EQ(df3->Max<int>("z").GetValue(), 42);

   gSystem->Unlink(fname);
   gSystem->Unlink(fname2);
}

TEST(RDFSnapshotMore, LazyNotTriggeredMT)
{
   ROOT::EnableImplicitMT(4);
   ROOT_EXPECT_WARNING(BookLazySnapshot(), "Snapshot",
                       "A lazy Snapshot action was booked but never triggered. The tree 't' in output file "
                       "'lazysnapshotnottriggered_shouldnotbecreated.root' was not created. "
                       "In case it was desired instead, remember to trigger the Snapshot operation, by "
                       "storing its result in a variable and for example calling the GetValue() method on it.");
   ROOT::DisableImplicitMT();
}

TEST(RDFSnapshotMore, LazyTriggeredMT)
{
   TIMTEnabler _(4);
   auto fname = "LazyTriggeredMT.root";
   ROOT_EXPECT_NODIAG(*ReturnLazySnapshot(fname));
   gSystem->Unlink(fname);
}

TEST(RDFSnapshotMore, EmptyBuffersMT)
{
   // Test that a tree gets created when only one worker yields events
   const auto fname = "emptybuffersmt.root";
   const auto treename = "t";
   const unsigned int nslots = std::min(4U, std::thread::hardware_concurrency());
   ROOT::EnableImplicitMT(nslots);
   ROOT::RDataFrame d(10);
   std::atomic_bool firstWorker{true};
   auto dd = d.DefineSlot("x", [&](unsigned int) {
                 bool expected = true;
                 if (firstWorker.compare_exchange_strong(expected, false)) {
                    return 0;
                 }
                 return 1;
              }).Filter([](int x) { return x == 0; }, {"x"}, "f");
   auto r = dd.Report();
   dd.Snapshot(treename, fname, {"x"});

   // check test sanity
   const auto passed = r->At("f").GetPass();
   EXPECT_GT(passed, 0u);

   // check result
   TFile f(fname);
   auto t = f.Get<TTree>(treename);
   EXPECT_EQ(t->GetListOfBranches()->GetEntries(), 1);
   EXPECT_EQ(t->GetEntries(), Long64_t(passed));

   ROOT::DisableImplicitMT();
   gSystem->Unlink(fname);
}

TEST(RDFSnapshotMore, ReadWriteCarrayMT)
{
   ROOT::EnableImplicitMT(4);
   ReadWriteCarray("ReadWriteCarrayMT");
   ROOT::DisableImplicitMT();
}

TEST(RDFSnapshotMore, TClonesArrayMT)
{
   TIMTEnabler _(4);
   ReadWriteTClonesArray();
}

// Test that we error out gracefully in case the output file specified for a Snapshot cannot be opened
TEST(RDFSnapshotMore, ForbiddenOutputFilenameMT)
{
   TIMTEnabler _(4);
   ROOT::RDataFrame df(4);
   const auto out_fname = "/definitely/not/a/valid/path/f.root";

   // Compiled
   try {
      const auto expected = "file /definitely/not/a/valid/path/f.root can not be opened No such file or directory";
      ROOT_EXPECT_SYSERROR(df.Snapshot("t", out_fname, {"rdfslot_"}), "TFile::TFile", expected);
   } catch (const std::runtime_error &e) {
      EXPECT_STREQ(e.what(), "Snapshot: could not create output file /definitely/not/a/valid/path/f.root");
   }

   // Jitted
   // the error printed here is
   // "SysError in <TFile::TFile>: file /definitely/not/a/valid/path/f.root can not be opened No such file or
   // directory\nError in <TReentrantRWLock::WriteUnLock>: Write lock already released for 0x55f179989378\n" but the
   // address printed changes every time
   ROOT::TestSupport::CheckDiagsRAII diagRAII;
   diagRAII.requiredDiag(kSysError, "TFile::TFile",
                         "file /definitely/not/a/valid/path/f.root can not be opened No such file or directory");
   diagRAII.optionalDiag(kSysError, "TReentrantRWLock::WriteUnLock", "Write lock already released for",
                         /*wholeStringNeedsToMatch=*/false);
   EXPECT_THROW(df.Snapshot("t", out_fname, {"rdfslot_"}), std::runtime_error);
}

/**
 * Test against issue #6523 and #6640
 * Try to force `TTree::ChangeFile` behaviour. Within RDataFrame, this should
 * not happen and both sequential and multithreaded Snapshot should only create
 * one file.
 */
TEST(RDFSnapshotMore, SetMaxTreeSizeMT)
{
   // Set TTree max size to a low number. Normally this would trigger the
   // behaviour of TTree::ChangeFile, but not within RDataFrame.
   const auto old_maxtreesize = TTree::GetMaxTreeSize();
   TTree::SetMaxTreeSize(1000);

   // Create TTree, fill it and Snapshot (should create one single file).
   {
      TTree t{"T", "SetMaxTreeSize(1000)"};
      int x{};
      const int nentries = 20000;

      t.Branch("x", &x, "x/I");

      for (auto i = 0; i < nentries; i++) {
         x = i;
         t.Fill();
      }

      ROOT::RDataFrame df{t};
      df.Snapshot("T", "rdfsnapshot_ttree_sequential_setmaxtreesize.root", {"x"});
   }

   // Create an RDF from the previously snapshotted file, then Snapshot again
   // with IMT enabled.
   {
      ROOT::EnableImplicitMT();

      ROOT::RDataFrame df{"T", "rdfsnapshot_ttree_sequential_setmaxtreesize.root"};
      df.Snapshot("T", "rdfsnapshot_imt_setmaxtreesize.root", {"x"});

      ROOT::DisableImplicitMT();
   }

   // Check the file for data integrity.
   {
      TFile f{"rdfsnapshot_imt_setmaxtreesize.root"};
      std::unique_ptr<TTree> t{f.Get<TTree>("T")};

      EXPECT_EQ(t->GetEntries(), 20000);

      int sum{0};
      int x{0};
      t->SetBranchAddress("x", &x);

      for (auto i = 0; i < t->GetEntries(); i++) {
         t->GetEntry(i);
         sum += x;
      }

      // sum(range(20000)) == 199990000
      EXPECT_EQ(sum, 199990000);
   }

   gSystem->Unlink("rdfsnapshot_ttree_sequential_setmaxtreesize.root");
   gSystem->Unlink("rdfsnapshot_imt_setmaxtreesize.root");

   // Reset TTree max size to its old value
   TTree::SetMaxTreeSize(old_maxtreesize);
}

TEST(RDFSnapshotMore, ZeroOutputEntriesMT)
{
   const auto fname = "snapshot_zerooutputentriesmt.root";
   ROOT::RDataFrame(10).Alias("c", "rdfentry_").Filter([] { return false; }).Snapshot("t", fname, {"c"});
   EXPECT_EQ(gSystem->AccessPathName(fname), 0); // This returns 0 if the file IS there

   TFile f(fname);
   auto *t = f.Get<TTree>("t");
   // TTree "t" should *not* be in there, differently from the single-thread case: see ROOT-10868
   EXPECT_NE(t, nullptr);
   gSystem->Unlink(fname);
}

TEST(RDFSnapshotMore, CustomBasketSizeMT)
{
   ROOT::EnableImplicitMT();
   TestCustomBasketSize();
   ROOT::DisableImplicitMT();
}

// Test for default basket size
TEST(RDFSnapshotMore, DefaultBasketSizeMT)
{
   ROOT::EnableImplicitMT();
   TestDefaultBasketSize();
   ROOT::DisableImplicitMT();
}

// Test for basket size preservation
TEST(RDFSnapshotMore, BasketSizePreservationMT)
{
   ROOT::EnableImplicitMT();
   TestBasketSizePreservation();
   ROOT::DisableImplicitMT();
}

#endif // R__USE_IMT
