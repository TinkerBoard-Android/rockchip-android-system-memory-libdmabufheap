/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <linux/ion_4.12.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <string>
#include <unordered_map>
#include <vector>

#include <android-base/unique_fd.h>

static constexpr char kDmabufSystemHeapName[] = "system";

class BufferAllocator {
  public:
    BufferAllocator();
    ~BufferAllocator() {}

    /* Not copyable or movable */
    BufferAllocator(const BufferAllocator&) = delete;
    BufferAllocator& operator=(const BufferAllocator&) = delete;

    /**
     * Maps a dmabuf heap to an equivalent ion heap configuration. This method is required since
     * dmabuf heaps do not support heap flags. This means that a single ion heap may encompass the
     * functionality of multiple dmabuf heaps by using heap flags. This method will check the
     * interface being used and only create the required mappings. For example,
     * if the interface being used is dmabuf heaps, the method will not do
     * anything. If the interface being used is non-legacy ion, the mapping from
     * dmabuf heap name to non-legacy ion heap name will be created and the
     * legacy parameters will be ignored.
     * The method can be deprecated once all devices have
     * migrated to dmabuf heaps from ion. Returns an error code when the
     * interface used is non-legacy ion and the @ion_heap_name parameter is
     * invalid or if the interface used is legacy ion and @legacy_ion_heap_mask
     * is invalid(0);
     * @heap_name: dmabuf heap name.
     * @ion_heap_name: name of the equivalent ion heap.
     * @ion_heap_flags: flags to be passed to the ion heap @ion_heap_name for it to function
     * equivalently to the dmabuf heap @heap_name.
     * @legacy_ion_heap_mask: heap mask for the equivalent legacy ion heap.
     * @legacy_ion_heap_flags: flags to be passed to the legacy ion heap for it
     * to function equivalently to dmabuf heap @heap_name..
     */
    int MapNameToIonHeap(const std::string& heap_name, const std::string& ion_heap_name,
                         unsigned int ion_heap_flags = 0, unsigned int legacy_ion_heap_mask = 0,
                         unsigned int legacy_ion_heap_flags = 0);

    /* *
     * Returns a dmabuf fd if the allocation in one of the specified heaps is successful and
     * an error code otherwise. If dmabuf heaps are supported, tries to allocate in the
     * specified dmabuf heap. If dmabuf heaps are not supported and if ion_fd is a valid fd,
     * go through saved heap data to find a heap ID/mask to match the specified heap names and
     * allocate memory as per the specified parameters. For vendor defined heaps with a legacy
     * ION interface(no heap query support), MapNameToIonMask() must be called prior to invocation
     * of Alloc() to map a heap name to an equivalent heap mask and heap flag configuration.
     * @heap_name: name of the heap to allocate in.
     * @len: size of the allocation.
     * @heap_flags: flags passed to heap.
     */
    int Alloc(const std::string& heap_name, size_t len, unsigned int heap_flags = 0);

  private:
    int OpenDmabufHeap(const std::string& name);
    void QueryIonHeaps();
    int GetDmabufHeapFd(const std::string& name);
    bool DmabufHeapsSupported() { return !dmabuf_heap_fds_.empty(); }
    int GetIonHeapIdByName(const std::string& heap_name, unsigned int* heap_id);
    int MapNameToIonMask(const std::string& heap_name, unsigned int ion_heap_mask,
                         unsigned int ion_heap_flags = 0);
    int MapNameToIonName(const std::string& heap_name, const std::string& ion_heap_name,
                         unsigned int ion_heap_flags = 0);
    void LogInterface(const std::string& interface);
    int IonAlloc(const std::string& heap_name, size_t len, unsigned int heap_flags = 0);
    int DmabufAlloc(const std::string& heap_name, size_t len);

    struct IonHeapConfig {
        unsigned int mask;
        unsigned int flags;
    };
    int GetIonConfig(const std::string& heap_name, IonHeapConfig& heap_config);

    /* Stores all open dmabuf_heap handles. */
    std::unordered_map<std::string, android::base::unique_fd> dmabuf_heap_fds_;

    /* saved handle to /dev/ion. */
    android::base::unique_fd ion_fd_;
    /**
     * Stores the queried ion heap data. Struct ion_heap_date is defined
     * as part of the ION UAPI as follows.
     * struct ion_heap_data {
     *   char name[MAX_HEAP_NAME];
     *    __u32 type;
     *    __u32 heap_id;
     *    __u32 reserved0;
     *    __u32 reserved1;
     *    __u32 reserved2;
     * };
     */
    bool uses_legacy_ion_iface_ = false;
    std::vector<struct ion_heap_data> ion_heap_info_;
    inline static bool logged_interface_ = false;
    /* stores a map of dmabuf heap names to equivalent ion heap configurations. */
    std::unordered_map<std::string, struct IonHeapConfig> heap_name_to_config_;
};
