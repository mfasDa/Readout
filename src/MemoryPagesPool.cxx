// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "MemoryPagesPool.h"

MemoryPagesPool::MemoryPagesPool(size_t vPageSize, size_t vNumberOfPages,
                                 void *vBaseAddress, size_t vBaseSize,
                                 ReleaseCallback vCallback,
                                 size_t firstPageOffset) {
  // initialize members from parameters
  pageSize = vPageSize;
  numberOfPages = vNumberOfPages;
  baseBlockAddress = vBaseAddress;
  baseBlockSize = vBaseSize;
  releaseBaseBlockCallback = vCallback;

  // if not specified, assuming base block size big enough to fit number of
  // pages * page size
  if (baseBlockSize == 0) {
    baseBlockSize = pageSize * numberOfPages;
  }

  // check validity of parameters
  if ((firstPageOffset >= vBaseSize) || (vBaseSize == 0) ||
      (vNumberOfPages == 0) || (vPageSize == 0) || (baseBlockSize == 0)) {
    throw __LINE__;
  }

  // if necessary, reduce number of pages to fit in available space
  size_t sizeNeeded = pageSize * numberOfPages + firstPageOffset;
  if (sizeNeeded > baseBlockSize) {
    numberOfPages = (baseBlockSize - firstPageOffset) / pageSize;
  }

  // create a fifo and store list of pages available
  pagesAvailable =
      std::make_unique<AliceO2::Common::Fifo<void *>>(numberOfPages);
  void *ptr = nullptr;
  for (size_t i = 0; i < numberOfPages; i++) {
    ptr = &((char *)baseBlockAddress)[firstPageOffset + i * pageSize];
    pagesAvailable->push(ptr);
    if (i == 0) {
      firstPageAddress = ptr;
    }
  }
  lastPageAddress = ptr;
}

MemoryPagesPool::~MemoryPagesPool() {
  // if defined, use provided callback to release base block
  if ((releaseBaseBlockCallback != nullptr) && (baseBlockAddress != nullptr)) {
    releaseBaseBlockCallback(baseBlockAddress);
  }
}

void *MemoryPagesPool::getPage() {
  // get a page from fifo, if available
  void *ptr = nullptr;
  pagesAvailable->pop(ptr);
  return ptr;
}

void MemoryPagesPool::releasePage(void *address) {
  // safety check on address provided
  if (!isPageValid(address)) {
    throw __LINE__;
  }

  // put back page in list of available pages
  pagesAvailable->push(address);
}

size_t MemoryPagesPool::getPageSize() { return pageSize; }

size_t MemoryPagesPool::getTotalNumberOfPages() { return numberOfPages; }

size_t MemoryPagesPool::getNumberOfPagesAvailable() {
  return pagesAvailable->getNumberOfUsedSlots();
}

void *MemoryPagesPool::getBaseBlockAddress() { return baseBlockAddress; }
size_t MemoryPagesPool::getBaseBlockSize() { return baseBlockSize; }

std::shared_ptr<DataBlockContainer>
MemoryPagesPool::getNewDataBlockContainer(void *newPage) {
  // get a new page if none provided
  if (newPage == nullptr) {
    // get a new page from the pool
    newPage = getPage();
    if (newPage == nullptr) {
      return nullptr;
    }
  } else {
    // safety check on address provided
    if (!isPageValid(newPage)) {
      throw __LINE__;
    }
  }

  // fill header at beginning of page
  // assuming payload is contiguous after header
  DataBlock *b = (DataBlock *)newPage;
  b->header.blockType = DataBlockType::H_BASE;
  b->header.headerSize = sizeof(DataBlockHeaderBase);
  b->header.dataSize = pageSize - sizeof(DataBlock);
  b->header.blockId = undefinedBlockId;
  b->header.linkId = undefinedLinkId;
  b->header.equipmentId = undefinedEquipmentId;
  b->header.timeframeId = undefinedTimeframeId;
  b->data = &(((char *)b)[sizeof(DataBlock)]);

  // define a function to put it back in pool after use
  auto releaseCallback = [this, newPage](void) -> void {
    this->releasePage(newPage);
    return;
  };

  // create a container and associate data page and release callback
  std::shared_ptr<DataBlockContainer> bc = std::make_shared<DataBlockContainer>(
      releaseCallback, (DataBlock *)newPage, pageSize);
  if (bc == nullptr) {
    releaseCallback();
    return nullptr;
  }

  // printf("create dbc %p with data=%p stored=%p\n",bc,newPage,bc->getData());

  return bc;
}

bool MemoryPagesPool::isPageValid(void *pagePtr) {
  if (pagePtr < firstPageAddress) {
    return false;
  }
  if (pagePtr > lastPageAddress) {
    return false;
  }
  if ((((char *)pagePtr - (char *)firstPageAddress) % pageSize) != 0) {
    return false;
  }
  return true;
}
