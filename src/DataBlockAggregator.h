// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include <Common/Configuration.h>
#include <Common/Fifo.h>
#include <Common/Thread.h>
#include <Common/Timer.h>

#include <Common/DataBlock.h>
#include <Common/DataBlockContainer.h>
#include <Common/DataSet.h>

#include <memory>
#include <queue>

using namespace AliceO2::Common;

/*
  DataBlockAggregator

  One "slicer" per equipment: data blocks are grouped in a "slice" of blocks
  having the same TF id.

  TODO: should be done per data source.
*/

struct PartialSlice {
  uint8_t linkId;
  uint64_t tfId;
  DataSetReference currentDataSet;
};

// a class to group blocks with same ID in slices
class DataBlockSlicer {

public:
  DataBlockSlicer();
  ~DataBlockSlicer();

  // append a new block to curent slice of corresponding link
  // returns the number of blocks in slice used
  int appendBlock(DataBlockContainerReference const &block);

  // get a slice, if available
  // if includeIncomplete is true, also retrieves current slice, even if
  // incomplete otherwise, only a complete slice is returned, if any when
  // iterated, returned in order of creation, older first
  DataSetReference getSlice(bool includeIncomplete = false);

private:
  /*
     uint64_t currentId; // common id of the blocks in current data set being
     built DataSetReference currentDataSet; // current data set being built
  */
  const unsigned int maxLinks = 8192;      // maximum number of links
  std::vector<PartialSlice> partialSlices; // slices being built (one per link)

  std::queue<DataSetReference>
      slices; // data sets which have been built and are complete

  // todo: add a timeout
};

class DataBlockAggregator {
public:
  DataBlockAggregator(AliceO2::Common::Fifo<DataSetReference> *output,
                      std::string name = "Aggregator");
  ~DataBlockAggregator();

  int addInput(
      std::shared_ptr<AliceO2::Common::Fifo<DataBlockContainerReference>>
          input); // add a FIFO to be used as input

  void start(); // starts processing thread
  void stop(int waitStopped =
                1); // stop processing thread (and possibly wait it terminates)

  int disableSlicing =
      0; // when set, slicer is disabled, data is just passed through

  static Thread::CallbackResult threadCallback(void *arg);

  Thread::CallbackResult executeCallback();

  bool doFlush = 0; // when set, flush slices including incomplete ones
                    // the flag is reset automatically when done

private:
  std::vector<
      std::shared_ptr<AliceO2::Common::Fifo<DataBlockContainerReference>>>
      inputs;
  AliceO2::Common::Fifo<DataSetReference> *output; // todo: unique_ptr

  std::unique_ptr<Thread> aggregateThread;
  AliceO2::Common::Timer incompletePendingTimer;
  int isIncompletePending;

  std::vector<DataBlockSlicer> slicers;
  int nextIndex = 0; // index of input channel to start with at next iteration
                     // to fill output fifo. not starting always from zero to
                     // avoid favorizing low-index channels.
  unsigned long long totalBlocksIn = 0; // number of blocks received from inputs
};
