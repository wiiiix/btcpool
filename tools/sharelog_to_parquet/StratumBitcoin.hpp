/*
 The MIT License (MIT)

 Copyright (c) [2016] [BTC.COM]

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */
#pragma once

#include <cmath>

#include "bitcoin/bitcoin.pb.h"
#include "StratumStatus.h"

struct ShareBitcoinBytesV1 {
public:
  enum Result {
    // make default 0 as REJECT, so code bug is unlikely to make false ACCEPT
    // shares
    REJECT = 0,
    ACCEPT = 1
  };

  uint64_t jobId_ = 0;
  int64_t workerHashId_ = 0;
  uint32_t ip_ = 0;
  int32_t userId_ = 0;
  uint64_t shareDiff_ = 0;
  uint32_t timestamp_ = 0;
  uint32_t blkBits_ = 0;
  int32_t result_ = 0;
  // Even if the field does not exist,
  // gcc will add the field as a padding
  // under the default memory alignment parameter.
  int32_t padding_ = 0;
};

static_assert(
    sizeof(ShareBitcoinBytesV1) == 48,
    "ShareBitcoinBytesV1 should be 48 bytes");

struct ShareBitcoinBytesV2 {
  uint32_t version_ = 0;
  uint32_t checkSum_ = 0;

  int64_t workerHashId_ = 0;
  int32_t userId_ = 0;
  int32_t status_ = 0;
  int64_t timestamp_ = 0;
  IpAddress ip_ = 0;

  uint64_t jobId_ = 0;
  uint64_t shareDiff_ = 0;
  uint32_t blkBits_ = 0;
  uint32_t height_ = 0;
  uint32_t nonce_ = 0;
  uint32_t sessionId_ = 0;

  uint32_t checkSum() const {
    uint64_t c = 0;

    c += (uint64_t)version_;
    c += (uint64_t)workerHashId_;
    c += (uint64_t)userId_;
    c += (uint64_t)status_;
    c += (uint64_t)timestamp_;
    c += (uint64_t)ip_.addrUint64[0];
    c += (uint64_t)ip_.addrUint64[1];
    c += (uint64_t)jobId_;
    c += (uint64_t)shareDiff_;
    c += (uint64_t)blkBits_;
    c += (uint64_t)height_;
    c += (uint64_t)nonce_;
    c += (uint64_t)sessionId_;

    return ((uint32_t)c) + ((uint32_t)(c >> 32));
  }
};

class ShareBitcoin : public sharebase::BitcoinMsg {
public:
  ShareBitcoin() {
    set_version(CURRENT_VERSION);
    set_workerhashid(0);
    set_userid(0);
    set_status(0);
    set_timestamp(0);
    set_ip("0.0.0.0");
    set_jobid(0);
    set_sharediff(0);
    set_blkbits(0);
    set_height(0);
    set_nonce(0);
    set_sessionid(0);
    set_versionmask(0);
  }

  ShareBitcoin(const ShareBitcoin &r) = default;
  ShareBitcoin &operator=(const ShareBitcoin &r) = default;

  bool SerializeToBuffer(string &data, uint32_t &size) const {
    size = ByteSize();
    data.resize(size);
    if (!SerializeToArray((uint8_t *)data.data(), size)) {
      DLOG(INFO) << "share SerializeToArray failed!";
      return false;
    }
    return true;
  }

  bool UnserializeWithVersion(const uint8_t *data, uint32_t size) {

    if (nullptr == data || size <= 0) {
      return false;
    }

    const uint8_t *payload = data;
    uint32_t version = *((uint32_t *)payload);

    if (version == CURRENT_VERSION) {
      if (!ParseFromArray(
              (const uint8_t *)(payload + sizeof(uint32_t)),
              size - sizeof(uint32_t))) {
        DLOG(INFO) << "share ParseFromArray failed!";
        return false;
      }
    } else if (
        version == BYTES_VERSION && size == sizeof(ShareBitcoinBytesV2)) {

      ShareBitcoinBytesV2 *share = (ShareBitcoinBytesV2 *)payload;

      if (share->checkSum() != share->checkSum_) {
        DLOG(INFO) << "checkSum mismatched! checkSum_: " << share->checkSum_
                   << ", checkSum(): " << share->checkSum();
        return false;
      }

      set_version(CURRENT_VERSION);
      set_workerhashid(share->workerHashId_);
      set_userid(share->userId_);
      set_status(share->status_);
      set_timestamp(share->timestamp_);
      set_ip(share->ip_.toString());
      set_jobid(share->jobId_);
      set_sharediff(share->shareDiff_);
      set_blkbits(share->blkBits_);
      set_height(share->height_);
      set_nonce(share->nonce_);
      set_sessionid(share->sessionId_);

    } else if (size == sizeof(ShareBitcoinBytesV1)) {
      ShareBitcoinBytesV1 *share = (ShareBitcoinBytesV1 *)payload;

      char ipStr[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(share->ip_), ipStr, INET_ADDRSTRLEN);

      set_version(CURRENT_VERSION);
      set_workerhashid(share->workerHashId_);
      set_userid(share->userId_);
      set_status(
          share->result_ == ShareBitcoinBytesV1::ACCEPT
              ? StratumStatus::ACCEPT
              : StratumStatus::REJECT_NO_REASON);
      set_timestamp(share->timestamp_);
      set_ip(ipStr);
      set_jobid(share->jobId_);
      set_sharediff(share->shareDiff_);
      set_blkbits(share->blkBits_);

      // There is no height in ShareBitcoinBytesV1, so it can only be assumed.
      // Note: BTCPool's SBTC support is outdated, so SBTC is not considered.

      // The block reward should be 12.5 on this height
      set_height(570000);
    } else {
      DLOG(INFO) << "unknow share received!";
      return false;
    }

    return true;
  }

  bool SerializeToArrayWithVersion(string &data, uint32_t &size) const {
    size = ByteSize();
    data.resize(size + sizeof(uint32_t));

    uint8_t *payload = (uint8_t *)data.data();
    *((uint32_t *)payload) = version();

    if (!SerializeToArray(payload + sizeof(uint32_t), size)) {
      DLOG(INFO) << "SerializeToArray failed!";
      return false;
    }

    size += sizeof(uint32_t);
    return true;
  }

  bool SerializeToArrayWithLength(string &data, uint32_t &size) const {
    size = ByteSize();
    data.resize(size + sizeof(uint32_t));

    *((uint32_t *)data.data()) = size;
    uint8_t *payload = (uint8_t *)data.data();

    if (!SerializeToArray(payload + sizeof(uint32_t), size)) {
      DLOG(INFO) << "SerializeToArray failed!";
      return false;
    }

    size += sizeof(uint32_t);
    return true;
  }

  size_t getsharelength() { return IsInitialized() ? ByteSize() : 0; }

public:
  const static uint32_t BYTES_VERSION = 0x00010003u;
  const static uint32_t CURRENT_VERSION = 0x00010004u;
};

//----------------------------------------------------

// bits to difficulty, from <https://en.bitcoin.it/wiki/Difficulty>

inline float fast_log(float val) {
  int *const exp_ptr = reinterpret_cast<int *>(&val);
  int x = *exp_ptr;
  const int log_2 = ((x >> 23) & 255) - 128;
  x &= ~(255 << 23);
  x += 127 << 23;
  *exp_ptr = x;

  val = ((-1.0f / 3) * val + 2) * val - 2.0f / 3;
  return ((val + log_2) * 0.69314718f);
}

static float difficulty(unsigned int bits) {
  if (bits == 0) {
    return 0;
  }
  static double max_body = fast_log(0x00ffff), scaland = fast_log(256);
  return exp(
      max_body - fast_log(bits & 0x00ffffff) +
      scaland * (0x1d - ((bits & 0xff000000) >> 24)));
}

//----------------------------------------------------

template <>
class ParquetWriterT<ShareBitcoin> : public ParquetWriter {
protected:
  int64_t *indexs_ = nullptr;
  int64_t *workerIds_ = nullptr;
  int32_t *userIds_ = nullptr;
  int32_t *status_ = nullptr;
  int64_t *timestamps_ = nullptr;
  // ip
  int64_t *jobIds_ = nullptr;
  int64_t *shareDiff_ = nullptr;
  float *networkDiff_ = nullptr;
  int32_t *height_ = nullptr;
  int32_t *nonce_ = nullptr;
  int32_t *sessionId_ = nullptr;
  int32_t *versionMask_ = nullptr;
  int32_t *extUserId_ = nullptr;
  float *diffReached_ = nullptr;

public:
  ParquetWriterT() {
    indexs_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    workerIds_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    userIds_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    status_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    timestamps_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    // ip
    jobIds_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    shareDiff_ = new int64_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    networkDiff_ = new float[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    height_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    nonce_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    sessionId_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    versionMask_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    extUserId_ = new int32_t[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
    diffReached_ = new float[DEFAULT_NUM_ROWS_PER_ROW_GROUP];
  }

  ~ParquetWriterT() {
    if (shareNum_ > 0) {
      flushShares();
    }

    if (indexs_)
      delete[] indexs_;
    if (workerIds_)
      delete[] workerIds_;
    if (userIds_)
      delete[] userIds_;
    if (status_)
      delete[] status_;
    if (timestamps_)
      delete[] timestamps_;
    if (jobIds_)
      delete[] jobIds_;
    if (shareDiff_)
      delete[] shareDiff_;
    if (networkDiff_)
      delete[] networkDiff_;
    if (height_)
      delete[] height_;
    if (nonce_)
      delete[] nonce_;
    if (sessionId_)
      delete[] sessionId_;
    if (versionMask_)
      delete[] versionMask_;
    if (extUserId_)
      delete[] extUserId_;
    if (diffReached_)
      delete[] diffReached_;
  }

protected:
  std::shared_ptr<GroupNode> setupSchema() override {
    parquet::schema::NodeVector fields;

    fields.push_back(
        PrimitiveNode::Make("index", Repetition::REQUIRED, Type::INT64));
    fields.push_back(
        PrimitiveNode::Make("worker_id", Repetition::REQUIRED, Type::INT64));
    fields.push_back(
        PrimitiveNode::Make("user_id", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("status", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("timestamp", Repetition::REQUIRED, Type::INT64));
    /*fields.push_back(PrimitiveNode::Make(
        "ip", Repetition::REQUIRED, Type::BYTE_ARRAY, LogicalType::UTF8));*/
    fields.push_back(
        PrimitiveNode::Make("job_id", Repetition::REQUIRED, Type::INT64));
    fields.push_back(
        PrimitiveNode::Make("share_diff", Repetition::REQUIRED, Type::INT64));
    fields.push_back(
        PrimitiveNode::Make("network_diff", Repetition::REQUIRED, Type::FLOAT));
    fields.push_back(
        PrimitiveNode::Make("height", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("nonce", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("session_id", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("version_mask", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("ext_user_id", Repetition::REQUIRED, Type::INT32));
    fields.push_back(
        PrimitiveNode::Make("diff_reached", Repetition::REQUIRED, Type::FLOAT));

    // Create a GroupNode named 'share_bitcoin' using the primitive nodes
    // defined above This GroupNode is the root node of the schema tree
    return std::static_pointer_cast<GroupNode>(
        GroupNode::Make("share_bitcoin", Repetition::REQUIRED, fields));
  }

  void flushShares() {
    DLOG(INFO) << "flush " << shareNum_ << " shares";

    // Create a RowGroupWriter instance
    auto rgWriter = fileWriter_->AppendRowGroup();

    // index
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, indexs_);

    // worker_id
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, workerIds_);

    // user_id
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, userIds_);

    // status
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, status_);

    // timestamp
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, timestamps_);

    // ip_long
    /*static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, ipLong_);*/

    // job_id
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, jobIds_);

    // share_diff
    static_cast<parquet::Int64Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, shareDiff_);

    // network_diff
    static_cast<parquet::FloatWriter *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, networkDiff_);

    // height
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, height_);

    // nonce
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, nonce_);

    // session_id
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, sessionId_);

    // version_mask
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, versionMask_);

    // ext_user_id
    static_cast<parquet::Int32Writer *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, extUserId_);

    // diff_reached
    static_cast<parquet::FloatWriter *>(rgWriter->NextColumn())
        ->WriteBatch(shareNum_, nullptr, nullptr, diffReached_);

    // Save current RowGroup
    rgWriter->Close();

    shareNum_ = 0;
  }

public:
  void addShare(const ShareBitcoin &share) {
    indexs_[shareNum_] = ++index_;
    workerIds_[shareNum_] = share.workerhashid();
    userIds_[shareNum_] = share.userid();
    status_[shareNum_] = share.status();
    timestamps_[shareNum_] = share.timestamp();
    // ip
    jobIds_[shareNum_] = share.jobid();
    shareDiff_[shareNum_] = share.sharediff();
    networkDiff_[shareNum_] = difficulty(share.blkbits());
    height_[shareNum_] = share.height();
    nonce_[shareNum_] = share.nonce();
    sessionId_[shareNum_] = share.sessionid();
    versionMask_[shareNum_] = share.versionmask();
    extUserId_[shareNum_] = share.extuserid();
    diffReached_[shareNum_] = difficulty(share.bitsreached());

    shareNum_++;

    if (shareNum_ >= DEFAULT_NUM_ROWS_PER_ROW_GROUP) {
      flushShares();
    }
  }
};
