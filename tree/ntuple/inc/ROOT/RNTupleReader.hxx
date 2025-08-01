/// \file ROOT/RNTupleReader.hxx
/// \ingroup NTuple
/// \author Jakob Blomer <jblomer@cern.ch>
/// \date 2024-02-20

/*************************************************************************
 * Copyright (C) 1995-2024, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_RNTupleReader
#define ROOT_RNTupleReader

#include <ROOT/RConfig.hxx> // for R__unlikely
#include <ROOT/REntry.hxx>
#include <ROOT/RError.hxx>
#include <ROOT/RNTupleDescriptor.hxx>
#include <ROOT/RNTupleMetrics.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleReadOptions.hxx>
#include <ROOT/RNTupleUtil.hxx>
#include <ROOT/RNTupleView.hxx>
#include <ROOT/RPageStorage.hxx>
#include <ROOT/RSpan.hxx>

#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

namespace ROOT {
class RNTuple;

/// Listing of the different options that can be printed by RNTupleReader::GetInfo()
enum class ENTupleInfo {
   kSummary,        // The RNTuple name, description, number of entries
   kStorageDetails, // size on storage, page sizes, compression factor, etc.
   kMetrics,        // internals performance counters, requires that EnableMetrics() was called
};

// clang-format off
/**
\class ROOT::RNTupleReader
\ingroup NTuple
\brief Reads RNTuple data from storage

The RNTupleReader provides access to data stored in the RNTuple binary format as C++ objects, using an RNTupleModel.
It infers this model from the RNTuple's on-disk metadata, or uses a model imposed by the user.
The latter case allows users to read into a specialized RNTuple model that covers
only a subset of the fields in the RNTuple. The RNTuple model is used when reading complete entries through LoadEntry().
Individual fields can be read as well by instantiating a tree view.

~~~ {.cpp}
#include <ROOT/RNTupleReader.hxx>
#include <iostream>

auto reader = ROOT::RNTupleReader::Open("myNTuple", "some/file.root");
std::cout << "myNTuple has " << reader->GetNEntries() << " entries\n";
~~~
*/
// clang-format on
class RNTupleReader {
private:
   /// Set as the page source's scheduler for parallel page decompression if implicit multi-threading (IMT) is on.
   /// Needs to be destructed after the page source is destructed (and thus be declared before)
   std::unique_ptr<Internal::RPageStorage::RTaskScheduler> fUnzipTasks;

   std::unique_ptr<Internal::RPageSource> fSource;
   /// Needs to be destructed before fSource
   std::unique_ptr<ROOT::RNTupleModel> fModel;
   /// We use a dedicated on-demand reader for Show(). Printing data uses all the fields
   /// from the full model even if the analysis code uses only a subset of fields. The display reader
   /// is a clone of the original reader.
   std::unique_ptr<RNTupleReader> fDisplayReader;
   /// The RNTuple descriptor in the page source is protected by a read-write lock. We don't expose that to the
   /// users of RNTupleReader::GetDescriptor().  Instead, if descriptor information is needed, we clone the
   /// descriptor.  Using the descriptor's generation number, we know if the cached descriptor is stale.
   /// Retrieving descriptor data from an RNTupleReader is supposed to be for testing and information purposes,
   /// not on a hot code path.
   std::optional<ROOT::RNTupleDescriptor> fCachedDescriptor;
   Experimental::Detail::RNTupleMetrics fMetrics;
   /// If not nullopt, these will be used when creating the model
   std::optional<ROOT::RNTupleDescriptor::RCreateModelOptions> fCreateModelOptions;

   RNTupleReader(std::unique_ptr<ROOT::RNTupleModel> model, std::unique_ptr<Internal::RPageSource> source,
                 const ROOT::RNTupleReadOptions &options);
   /// The model is generated from the RNTuple metadata on storage.
   explicit RNTupleReader(std::unique_ptr<Internal::RPageSource> source, const ROOT::RNTupleReadOptions &options);

   void ConnectModel(ROOT::RNTupleModel &model);
   RNTupleReader *GetDisplayReader();
   void InitPageSource(bool enableMetrics);

   ROOT::DescriptorId_t RetrieveFieldId(std::string_view fieldName) const;

public:
   // Browse through the entries
   class RIterator {
   private:
      ROOT::NTupleSize_t fIndex = ROOT::kInvalidNTupleIndex;

   public:
      using iterator = RIterator;
      using iterator_category = std::forward_iterator_tag;
      using value_type = ROOT::NTupleSize_t;
      using difference_type = ROOT::NTupleSize_t;
      using pointer = ROOT::NTupleSize_t *;
      using reference = ROOT::NTupleSize_t &;

      RIterator() = default;
      explicit RIterator(ROOT::NTupleSize_t index) : fIndex(index) {}
      ~RIterator() = default;

      iterator operator++(int) /* postfix */
      {
         auto r = *this;
         fIndex++;
         return r;
      }
      iterator &operator++() /* prefix */
      {
         ++fIndex;
         return *this;
      }
      reference operator*() { return fIndex; }
      pointer operator->() { return &fIndex; }
      bool operator==(const iterator &rh) const { return fIndex == rh.fIndex; }
      bool operator!=(const iterator &rh) const { return fIndex != rh.fIndex; }
   };

   /// Open an RNTuple for reading.
   ///
   /// Throws an RException if there is no RNTuple with the given name.
   ///
   /// **Example: open an RNTuple and print the number of entries**
   /// ~~~ {.cpp}
   /// #include <ROOT/RNTupleReader.hxx>
   /// #include <iostream>
   ///
   /// auto reader = ROOT::RNTupleReader::Open("myNTuple", "some/file.root");
   /// std::cout << "myNTuple has " << reader->GetNEntries() << " entries\n";
   /// ~~~
   static std::unique_ptr<RNTupleReader> Open(std::string_view ntupleName, std::string_view storage,
                                              const ROOT::RNTupleReadOptions &options = ROOT::RNTupleReadOptions());
   static std::unique_ptr<RNTupleReader>
   Open(const RNTuple &ntuple, const ROOT::RNTupleReadOptions &options = ROOT::RNTupleReadOptions());

   /// The caller imposes a model, which must be compatible with the model found in the data on storage.
   static std::unique_ptr<RNTupleReader> Open(std::unique_ptr<ROOT::RNTupleModel> model, std::string_view ntupleName,
                                              std::string_view storage,
                                              const ROOT::RNTupleReadOptions &options = ROOT::RNTupleReadOptions());
   static std::unique_ptr<RNTupleReader> Open(std::unique_ptr<ROOT::RNTupleModel> model, const RNTuple &ntuple,
                                              const ROOT::RNTupleReadOptions &options = ROOT::RNTupleReadOptions());

   /// The caller imposes the way the model is reconstructed
   static std::unique_ptr<RNTupleReader> Open(const ROOT::RNTupleDescriptor::RCreateModelOptions &createModelOpts,
                                              std::string_view ntupleName, std::string_view storage,
                                              const ROOT::RNTupleReadOptions &options = ROOT::RNTupleReadOptions());
   static std::unique_ptr<RNTupleReader> Open(const ROOT::RNTupleDescriptor::RCreateModelOptions &createModelOpts,
                                              const RNTuple &ntuple,
                                              const ROOT::RNTupleReadOptions &options = ROOT::RNTupleReadOptions());
   std::unique_ptr<RNTupleReader> Clone()
   {
      auto options = ROOT::RNTupleReadOptions{};
      options.SetEnableMetrics(fMetrics.IsEnabled());
      return std::unique_ptr<RNTupleReader>(new RNTupleReader(fSource->Clone(), options));
   }
   ~RNTupleReader();

   ROOT::NTupleSize_t GetNEntries() const { return fSource->GetNEntries(); }
   const ROOT::RNTupleModel &GetModel();
   std::unique_ptr<ROOT::REntry> CreateEntry();

   /// Returns a cached copy of the page source descriptor. The returned pointer remains valid until the next call
   /// to LoadEntry() or to any of the views returned from the reader.
   const ROOT::RNTupleDescriptor &GetDescriptor();

   /// Prints a detailed summary of the RNTuple, including a list of fields.
   ///
   /// **Example: print summary information to stdout**
   /// ~~~ {.cpp}
   /// #include <ROOT/RNTupleReader.hxx>
   /// #include <iostream>
   ///
   /// auto reader = ROOT::RNTupleReader::Open("myNTuple", "some/file.root");
   /// reader->PrintInfo();
   /// // or, equivalently:
   /// reader->PrintInfo(ROOT::ENTupleInfo::kSummary, std::cout);
   /// ~~~
   /// **Example: print detailed column storage data to stderr**
   /// ~~~ {.cpp}
   /// #include <ROOT/RNTupleReader.hxx>
   /// #include <iostream>
   ///
   /// auto reader = ROOT::RNTupleReader::Open("myNTuple", "some/file.root");
   /// reader->PrintInfo(ROOT::ENTupleInfo::kStorageDetails, std::cerr);
   /// ~~~
   ///
   /// For use of ENTupleInfo::kMetrics, see #EnableMetrics.
   void PrintInfo(const ENTupleInfo what = ENTupleInfo::kSummary, std::ostream &output = std::cout) const;

   /// Shows the values of the i-th entry/row, starting with 0 for the first entry. By default,
   /// prints the output in JSON format.
   /// Uses the visitor pattern to traverse through each field of the given entry.
   void Show(ROOT::NTupleSize_t index, std::ostream &output = std::cout);

   /// Fills the default entry of the model.
   /// Raises an exception when `index` is greater than the number of entries present in the RNTuple
   void LoadEntry(ROOT::NTupleSize_t index)
   {
      // TODO(jblomer): can be templated depending on the factory method / constructor
      if (R__unlikely(!fModel)) {
         // Will create the fModel.
         GetModel();
      }
      LoadEntry(index, fModel->GetDefaultEntry());
   }
   /// Fills a user provided entry after checking that the entry has been instantiated from the RNTuple model
   void LoadEntry(ROOT::NTupleSize_t index, ROOT::REntry &entry)
   {
      if (R__unlikely(entry.GetModelId() != fModel->GetModelId()))
         throw RException(R__FAIL("mismatch between entry and model"));

      entry.Read(index);
   }

   /// Returns an iterator over the entry indices of the RNTuple.
   ///
   /// **Example: iterate over all entries and print each entry in JSON format**
   /// ~~~ {.cpp}
   /// #include <ROOT/RNTupleReader.hxx>
   /// #include <iostream>
   ///
   /// auto reader = ROOT::RNTupleReader::Open("myNTuple", "some/file.root");
   /// for (auto i : ntuple->GetEntryRange()) {
   ///    reader->Show(i);
   /// }
   /// ~~~
   ROOT::RNTupleGlobalRange GetEntryRange() { return ROOT::RNTupleGlobalRange(0, GetNEntries()); }

   /// Provides access to an individual (sub)field,
   /// e.g. `GetView<Particle>("particle")`, `GetView<double>("particle.pt")` or
   /// `GetView<std::vector<Particle>>("particles")`. It is possible to directly get the size of a collection (without
   /// reading the collection itself) using RNTupleCardinality:
   /// `GetView<ROOT::RNTupleCardinality<std::uint64_t>>("particles")`.
   ///
   /// Raises an exception if there is no field with the given name.
   ///
   /// **Example: iterate over a field named "pt" of type `float`**
   /// ~~~ {.cpp}
   /// #include <ROOT/RNTupleReader.hxx>
   /// #include <iostream>
   ///
   /// auto reader = ROOT::RNTupleReader::Open("myNTuple", "some/file.root");
   /// auto pt = reader->GetView<float>("pt");
   ///
   /// for (auto i : reader->GetEntryRange()) {
   ///    std::cout << i << ": " << pt(i) << "\n";
   /// }
   /// ~~~
   ///
   /// **Note**: if `T = void`, type checks are disabled. This is not really useful for this overload because
   /// RNTupleView<void> does not give access to the pointer. If required, it is possible to provide an `objPtr` of a
   /// dynamic type, for example via GetView(std::string_view, void *, std::string_view).
   template <typename T>
   ROOT::RNTupleView<T> GetView(std::string_view fieldName)
   {
      return GetView<T>(RetrieveFieldId(fieldName));
   }

   /// Provides access to an individual (sub)field, reading its values into `objPtr`.
   ///
   /// Raises an exception if there is no field with the given name.
   ///
   /// **Example: iterate over a field named "pt" of type `float`**
   /// ~~~ {.cpp}
   /// #include <ROOT/RNTupleReader.hxx>
   /// #include <iostream>
   ///
   /// auto reader = ROOT::RNTupleReader::Open("myNTuple", "some/file.root");
   /// auto pt = std::make_shared<float>();
   /// auto ptView = reader->GetView("pt", pt);
   ///
   /// for (auto i : reader->GetEntryRange()) {
   ///    ptView(i);
   ///    std::cout << i << ": " << *pt << "\n";
   /// }
   /// ~~~
   ///
   /// **Note**: if `T = void`, type checks are disabled. It is the caller's responsibility to match the field and
   /// object types. It is strongly recommended to use an overload that allows passing the `typeName`, such as
   /// GetView(std::string_view, void *, std::string_view). This allows type checks with the on-disk metadata and
   /// enables automatic schema evolution and conversion rules.
   template <typename T>
   ROOT::RNTupleView<T> GetView(std::string_view fieldName, std::shared_ptr<T> objPtr)
   {
      return GetView<T>(RetrieveFieldId(fieldName), objPtr);
   }

   /// Provides access to an individual (sub)field, reading its values into `rawPtr`.
   ///
   /// \sa GetView(std::string_view, std::shared_ptr<T>)
   template <typename T>
   ROOT::RNTupleView<T> GetView(std::string_view fieldName, T *rawPtr)
   {
      return GetView<T>(RetrieveFieldId(fieldName), rawPtr);
   }

   /// Provides access to an individual (sub)field, reading its values into `rawPtr` as the type provided by `typeName`.
   ///
   /// \sa GetView(std::string_view, std::shared_ptr<T>)
   ROOT::RNTupleView<void> GetView(std::string_view fieldName, void *rawPtr, std::string_view typeName)
   {
      return GetView(RetrieveFieldId(fieldName), rawPtr, typeName);
   }

   /// Provides access to an individual (sub)field, reading its values into `rawPtr` as the type provided by `ti`.
   ///
   /// \sa GetView(std::string_view, std::shared_ptr<T>)
   ROOT::RNTupleView<void> GetView(std::string_view fieldName, void *rawPtr, const std::type_info &ti)
   {
      return GetView(RetrieveFieldId(fieldName), rawPtr, ROOT::Internal::GetRenormalizedTypeName(ti));
   }

   /// Provides access to an individual (sub)field from its on-disk ID.
   ///
   /// \sa GetView(std::string_view)
   template <typename T>
   ROOT::RNTupleView<T> GetView(ROOT::DescriptorId_t fieldId)
   {
      auto field = ROOT::RNTupleView<T>::CreateField(fieldId, *fSource);
      auto range = ROOT::Internal::GetFieldRange(*field, *fSource);
      return ROOT::RNTupleView<T>(std::move(field), range);
   }

   /// Provides access to an individual (sub)field from its on-disk ID, reading its values into `objPtr`.
   ///
   /// \sa GetView(std::string_view, std::shared_ptr<T>)
   template <typename T>
   ROOT::RNTupleView<T> GetView(ROOT::DescriptorId_t fieldId, std::shared_ptr<T> objPtr)
   {
      auto field = ROOT::RNTupleView<T>::CreateField(fieldId, *fSource);
      auto range = ROOT::Internal::GetFieldRange(*field, *fSource);
      return ROOT::RNTupleView<T>(std::move(field), range, objPtr);
   }

   /// Provides access to an individual (sub)field from its on-disk ID, reading its values into `rawPtr`.
   ///
   /// \sa GetView(std::string_view, std::shared_ptr<T>)
   template <typename T>
   ROOT::RNTupleView<T> GetView(ROOT::DescriptorId_t fieldId, T *rawPtr)
   {
      auto field = ROOT::RNTupleView<T>::CreateField(fieldId, *fSource);
      auto range = ROOT::Internal::GetFieldRange(*field, *fSource);
      return ROOT::RNTupleView<T>(std::move(field), range, rawPtr);
   }

   /// Provides access to an individual (sub)field from its on-disk ID, reading its values into `rawPtr` as the type
   /// provided by `typeName`.
   ///
   /// \sa GetView(std::string_view, std::shared_ptr<T>)
   ROOT::RNTupleView<void> GetView(ROOT::DescriptorId_t fieldId, void *rawPtr, std::string_view typeName)
   {
      auto field = RNTupleView<void>::CreateField(fieldId, *fSource, typeName);
      auto range = ROOT::Internal::GetFieldRange(*field, *fSource);
      return RNTupleView<void>(std::move(field), range, rawPtr);
   }

   /// Provides access to an individual (sub)field from its on-disk ID, reading its values into `objPtr` as the type
   /// provided by `ti`.
   ///
   /// \sa GetView(std::string_view, std::shared_ptr<T>)
   ROOT::RNTupleView<void> GetView(ROOT::DescriptorId_t fieldId, void *rawPtr, const std::type_info &ti)
   {
      return GetView(fieldId, rawPtr, ROOT::Internal::GetRenormalizedTypeName(ti));
   }

   /// Provides direct access to the I/O buffers of a **mappable** (sub)field.
   ///
   /// Raises an exception if there is no field with the given name.
   /// Attempting to access the values of a direct-access view for non-mappable fields will yield compilation errors.
   ///
   /// \sa GetView(std::string_view)
   template <typename T>
   ROOT::RNTupleDirectAccessView<T> GetDirectAccessView(std::string_view fieldName)
   {
      return GetDirectAccessView<T>(RetrieveFieldId(fieldName));
   }

   /// Provides direct access to the I/O buffers of a **mappable** (sub)field from its on-disk ID.
   ///
   /// \sa GetDirectAccessView(std::string_view)
   template <typename T>
   ROOT::RNTupleDirectAccessView<T> GetDirectAccessView(ROOT::DescriptorId_t fieldId)
   {
      auto field = ROOT::RNTupleDirectAccessView<T>::CreateField(fieldId, *fSource);
      auto range = ROOT::Internal::GetFieldRange(field, *fSource);
      return ROOT::RNTupleDirectAccessView<T>(std::move(field), range);
   }

   /// Provides access to a collection field, that can itself generate new RNTupleViews for its nested fields.
   ///
   /// Raises an exception if:
   /// * there is no field with the given name or,
   /// * the field is not a collection
   ///
   /// \sa GetView(std::string_view)
   ROOT::RNTupleCollectionView GetCollectionView(std::string_view fieldName)
   {
      auto fieldId = fSource->GetSharedDescriptorGuard()->FindFieldId(fieldName);
      if (fieldId == ROOT::kInvalidDescriptorId) {
         throw RException(R__FAIL("no field named '" + std::string(fieldName) + "' in RNTuple '" +
                                  fSource->GetSharedDescriptorGuard()->GetName() + "'"));
      }
      return GetCollectionView(fieldId);
   }

   /// Provides access to a collection field from its on-disk ID, that can itself generate new RNTupleViews for its
   /// nested fields.
   ///
   /// \sa GetCollectionView(std::string_view)
   ROOT::RNTupleCollectionView GetCollectionView(ROOT::DescriptorId_t fieldId)
   {
      return ROOT::RNTupleCollectionView::Create(fieldId, fSource.get());
   }

   RIterator begin() { return RIterator(0); }
   RIterator end() { return RIterator(GetNEntries()); }

   /// Enable performance measurements (decompression time, bytes read from storage, etc.)
   ///
   /// **Example: inspect the reader metrics after loading every entry**
   /// ~~~ {.cpp}
   /// #include <ROOT/RNTupleReader.hxx>
   /// #include <iostream>
   ///
   /// auto ntuple = ROOT::RNTupleReader::Open("myNTuple", "some/file.root");
   /// // metrics must be turned on beforehand
   /// reader->EnableMetrics();
   ///
   /// for (auto i : ntuple->GetEntryRange()) {
   ///    reader->LoadEntry(i);
   /// }
   /// reader->PrintInfo(ROOT::ENTupleInfo::kMetrics);
   /// ~~~
   void EnableMetrics() { fMetrics.Enable(); }
   const Experimental::Detail::RNTupleMetrics &GetMetrics() const { return fMetrics; }
}; // class RNTupleReader

} // namespace ROOT

#endif // ROOT_RNTupleReader
