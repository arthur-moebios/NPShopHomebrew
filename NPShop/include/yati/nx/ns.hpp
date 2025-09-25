#pragma once

#include <switch.h>
#include "ncm.hpp"

namespace npshop::ns {

enum ApplicationRecordType {
    ApplicationRecordType_Running         = 0x0,
    ApplicationRecordType_Installed       = 0x3,
    ApplicationRecordType_Downloading     = 0x4,
    // application is gamecard, but gamecard isn't insterted
    ApplicationRecordType_GamecardMissing = 0x5,
    ApplicationRecordType_Downloaded      = 0x6,
    ApplicationRecordType_Updated         = 0xA,
    ApplicationRecordType_Archived        = 0xB,
};

Result PushApplicationRecord(Service* srv, u64 tid, const ncm::ContentStorageRecord* records, u32 count);
Result ListApplicationRecordContentMeta(Service* srv, u64 offset, u64 tid, ncm::ContentStorageRecord* out_records, u32 count, s32* entries_read);
Result DeleteApplicationRecord(Service* srv, u64 tid);
Result InvalidateApplicationControlCache(Service* srv, u64 tid);

} // namespace npshop::ns
