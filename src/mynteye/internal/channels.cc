#include "mynteye/internal/channels.h"

// #include <sys/time.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iterator>
#include <chrono>
#include <stdexcept>

#include "mynteye/util/log.h"
#include "mynteye/util/strings.h"

#define PACKET_SIZE 64
#define DATA_SIZE 15

MYNTEYE_BEGIN_NAMESPACE

namespace {

inline std::uint8_t check_sum(std::uint8_t *buf, std::uint16_t length) {
  std::uint8_t crc8 = 0;
  while (length--) {
    crc8 = crc8 ^ (*buf++);
  }
  return crc8;
}

void CheckSpecVersion(const Version *spec_version) {
  if (spec_version == nullptr) {
    LOGE("%s %d:: Spec version must be specified.", __FILE__, __LINE__);
    return;
  }

  std::vector<std::string> spec_versions{"1.0"};
  for (auto &&spec_ver : spec_versions) {
    if (*spec_version == Version(spec_ver)) {
      return;  // supported
    }
  }

  std::ostringstream ss;
  std::copy(
      spec_versions.begin(), spec_versions.end(),
      std::ostream_iterator<std::string>(ss, ","));
  LOGE("%s %d:: Spec version %s not supported, must in [%s]",
      __FILE__, __LINE__, spec_version->to_string().c_str(),
      ss.str().c_str());
}

} // namespace

Channels::Channels() : is_hid_tracking_(false),
  hid_track_stop_(false),
  imu_callback_(nullptr),
  img_callback_(nullptr),
  req_count_(0) {
    device_ = std::make_shared<hid::hid_device>();
}

Channels::~Channels() {
  StopHidTracking();
}

void Channels::SetImuCallback(imu_callback_t callback) {
  imu_callback_ = callback;
}

void Channels::SetImgInfoCallback(img_callback_t callback) {
  img_callback_ = callback;
}

void Channels::DoHidTrack() {
  ImuResPacket imu_res_packet;
  ImgInfoResPacket img_res_packet;

  if (!ExtractHidData(imu_res_packet, img_res_packet)) {
    return;
  }

  if (imu_callback_ && img_callback_) {
    for (auto &&imu_packet : imu_res_packet.packets) {
      imu_callback_(imu_packet);
    }
    for (auto &&img_packet : img_res_packet.packets) {
      img_callback_(img_packet);
    }
  }
}

void Channels::Open() {
  is_hid_tracking_ = true;
  // open device
  if (device_->open(1, -1, -1) < 0) {
    if (device_->open(1, -1, -1) < 0) {
      LOGE("Error:: open imu device is failure.");
      return;
    }
  }
}

void Channels::StartHidTracking() {
  if (!is_hid_tracking_) {
    LOGE("Error:: imu device was opened already.");
    return;
  }

  /*
  is_hid_tracking_ = true;
  // open device
  if (device_->open(1, -1, -1) < 0) {
    if (device_->open(1, -1, -1) < 0) {
      LOGE("Error:: open imu device is failure.");
      return;
    }
  }
  */

  hid_track_thread_ = std::thread([this]() {
    while (!hid_track_stop_) {
      DoHidTrack();
    }
  });
}

bool Channels::ExtractHidData(ImuResPacket &imu, ImgInfoResPacket &img) {
  std::uint8_t data[PACKET_SIZE * 2]{};
  std::fill(data, data + PACKET_SIZE * 2, 0);

  int size = device_->receive(0, data, PACKET_SIZE * 2, 220);
  if (size < 0) {
    hid_track_stop_ = true;
    LOGE("Error:: Reading, device went offline !");
    return false;
  }


  for (int i = 0; i < size / PACKET_SIZE; i++) {
    std::uint8_t *packet = data + i * PACKET_SIZE;

    if (packet[PACKET_SIZE - 1] !=
        check_sum(&packet[3], packet[2])) {
      LOGW("check droped.");
      continue;
    }

    auto sn = *packet | *(packet + 1) << 8;
    if (package_sn_ == sn) { continue; }
    package_sn_ = sn;

    for (int offset = 3; offset <= PACKET_SIZE - DATA_SIZE;
        offset += DATA_SIZE) {
      if (*(packet + offset) == 2) {
        img.from_data(packet + offset);
      } else if (*(packet + offset) == 0 ||
          *(packet + offset) == 1) {
        imu.from_data(packet + offset);
      }
    }
  }

  return true;
}

bool Channels::StopHidTracking() {
  if (hid_track_stop_) {
    return false;
  }
  if (hid_track_thread_.joinable()) {
    hid_track_stop_ = true;
    hid_track_thread_.join();
    is_hid_tracking_ = false;
    hid_track_stop_ = false;
  }

  return true;
}

namespace {

template <typename T>
T _from_data(const std::uint8_t *data) {
  std::size_t size = sizeof(T) / sizeof(std::uint8_t);
  T value = 0;
  for (std::size_t i = 0; i < size; i++) {
    // value |= data[i] << (8 * (size - i - 1));
    value |= data[i] << (8 * i);
  }
  return value;
}

template<>
double _from_data(const std::uint8_t *data) {
  return *(reinterpret_cast<const double *>(data));
}

std::string _from_data(const std::uint8_t *data, std::size_t count) {
  std::string s(reinterpret_cast<const char *>(data), count);
  strings::trim(s);
  return s;
}

std::size_t from_data(Channels::device_info_t *info, const std::uint8_t *data) {
  std::size_t i = 4;  // skip vid, pid
  // name, 20
  info->name = _from_data(data + i, 20);
  i += 20;
  // serial_number, 24
  info->serial_number = _from_data(data + i, 24);
  i += 24;
  // firmware_version, 2
  info->firmware_version.set_minor(data[i]);
  info->firmware_version.set_major(data[i + 1]);
  i += 2;
  // hardware_version, 3
  info->hardware_version.set_minor(data[i]);
  info->hardware_version.set_major(data[i + 1]);
  info->hardware_version.set_flag(std::bitset<8>(data[i + 2]));
  i += 3;
  // spec_version, 2
  info->spec_version.set_minor(data[i]);
  info->spec_version.set_major(data[i + 1]);
  i += 2;
  // lens_type, 4
  info->lens_type.set_vendor(_from_data<std::uint16_t>(data + i));
  info->lens_type.set_product(_from_data<std::uint16_t>(data + i + 2));
  i += 4;
  // imu_type, 4
  info->imu_type.set_vendor(_from_data<std::uint16_t>(data + i));
  info->imu_type.set_product(_from_data<std::uint16_t>(data + i + 2));
  i += 4;
  // nominal_baseline, 2
  info->nominal_baseline = _from_data<std::uint16_t>(data + i);
  i += 2;

  return i;
}

std::size_t from_data(ImuIntrinsics *in,
    const std::uint8_t *data, const Version *spec_version) {
  std::size_t i = 0;

  // scale
  for (std::size_t j = 0; j < 3; j++) {
    for (std::size_t k = 0; k < 3; k++) {
      in->scale[j][k] = _from_data<double>(data + i + (j * 3 + k) * 8);
    }
  }
  i += 72;
  // assembly
  for (std::size_t j = 0; j < 3; j++) {
    for (std::size_t k = 0; k < 3; k++) {
      in->assembly[j][k] = _from_data<double>(data + i + (j * 3 + k) * 8);
    }
  }
  i += 72;
  // drift
  for (std::size_t j = 0; j < 3; j++) {
    in->drift[j] = _from_data<double>(data + i + j * 8);
  }
  i += 24;
  // noise
  for (std::size_t j = 0; j < 3; j++) {
    in->noise[j] = _from_data<double>(data + i + j * 8);
  }
  i += 24;
  // bias
  for (std::size_t j = 0; j < 3; j++) {
    in->bias[j] = _from_data<double>(data + i + j * 8);
  }
  i += 24;
  i += 100;
  // warm drift
  // x
  for (std::size_t j = 0; j < 2; j++) {
    in->x[j] = _from_data<double>(data + i + j * 8);
  }
  i += 16;
  // y
  for (std::size_t j = 0; j < 2; j++) {
    in->y[j] = _from_data<double>(data + i + j * 8);
  }
  i += 16;
  // z
  for (std::size_t j = 0; j < 2; j++) {
    in->z[j] = _from_data<double>(data + i + j * 8);
  }
  i += 16;

  UNUSED(spec_version);
  return i;
}

std::size_t from_data(Extrinsics *ex,
    const std::uint8_t *data, const Version *spec_version) {
  std::size_t i = 0;

  // rotation
  for (std::size_t j = 0; j < 3; j++) {
    for (std::size_t k = 0; k < 3; k++) {
      ex->rotation[j][k] = _from_data<double>(data + i + (j * 3 + k) * 8);
    }
  }
  i += 72;
  // translation
  for (std::size_t j = 0; j < 3; j++) {
    ex->translation[j] = _from_data<double>(data + i + j * 8);
  }
  i += 24;

  UNUSED(spec_version);
  return i;
}

std::size_t from_data(
    Channels::imu_params_t *imu_params,
    const std::uint8_t *data,
    const Version *spec_version) {
  std::size_t i = 0;
  i += from_data(&imu_params->in_accel, data + i, spec_version);
  i += from_data(&imu_params->in_gyro, data + i, spec_version);
  i += from_data(&imu_params->ex_left_to_imu, data + i, spec_version);
  return i;
}

} // namespace

bool Channels::RequireFileData(bool device_info,
    bool reserve,
    bool imu_params,
    std::uint8_t *data,
    std::uint16_t &file_size) {

  std::uint8_t buffer[64]{};

  buffer[0] = 0x0A;
  buffer[1] = 1;
  buffer[2] = 0x07 & ((device_info << 0)
      | (reserve << 1) | (imu_params << 2));

  if (device_->get_device_class() == 0xFF) {
    LOGE("%s %d:: Not support filechannel, please update firmware.",
        __FILE__, __LINE__);
    return false;
  }

  if (device_->send(0, buffer, 64, 200) <= 0) {
    LOGE("%s %d:: Send commend of imu instrinsics failed.",
        __FILE__, __LINE__);
    return false;
  }

  while (buffer[0] != 0x0B) {
    device_->receive(0, buffer, 64, 2000);
    if (++req_count_ > 5) {
      LOGE("%s %d:: Error reading, device went offline.",
          __FILE__, __LINE__);
      return false;
    }
  }

  std::uint32_t packets_sum = 0;
  std::int64_t packets_num = -1;
  std::uint32_t packets_index = 0;
  std::uint8_t *seek = data;
  while (true) {
    int ret = device_->receive(0, buffer, 64, 220);
    if (ret <= 0) {
      LOGE("%s %d:: Require imu instrinsics failed.",
          __FILE__, __LINE__);
      return false;
    }

    if (buffer[0] == 0x0B && (packets_num == -1)) { continue; }
    if (packets_num == (buffer[0] | buffer[1] << 8)) { continue; }
    if (((buffer[0] | (buffer[1] << 8)) - packets_num) > 1) {
      LOGE("%s %d:: Lost index of %d packets, please retry. %ld",
          __FILE__, __LINE__, ((buffer[0] | buffer[1] << 8) - 1), packets_num);
      return false;
    }

    packets_num = buffer[0] | buffer[1] << 8;

    std::uint8_t length = buffer[2];
    if (length <= 0) { return false; }

    if (buffer[3 + length] != check_sum(&buffer[3], length)) {
      LOGE("%s %d:: Check error. please retry.",
          __FILE__, __LINE__);
      return false;
    }

    if (packets_num == 0) {
      packets_sum = 4 + (buffer[4] | (buffer[5] << 8));
      packets_index = 0;
    }
    packets_index += length;
    std::copy(buffer + 3, buffer + 3 + length, seek);
    seek += length;
    if (packets_index >= packets_sum) {
      file_size = packets_index;
      break;
    }
  }

  return true;
}

bool Channels::GetFiles(device_info_t *info,
    imu_params_t *imu_params, Version *spec_version) {
  if (info == nullptr && imu_params == nullptr) {
    LOGE("%s %d:: Files are not provided to get.",
        __FILE__, __LINE__);
    return false;
  }

  std::uint8_t data[2000]{};
  std::uint16_t file_len;

  if (!RequireFileData(true, true, true, data, file_len)) {
    LOGE("%s %d:: GetFiles failed.", __FILE__, __LINE__);
    return false;
  }

  std::uint16_t size = _from_data<std::uint16_t>(data + 1);
  std::uint8_t checksum = data[3 + size];

  std::uint8_t checksum_now = check_sum(data + 3, size);
  if (checksum != checksum_now) {
    LOGW("%s %d:: Files checksum should be %x, but %x now", __FILE__,
        __LINE__, static_cast<int>(checksum), static_cast<int>(checksum_now));
    return false;
  }

  Version *spec_ver = spec_version;
  std::size_t i = 3;
  std::size_t end = 3 + size;
  while (i < end) {
    std::uint8_t file_id = *(data + i);
    std::uint16_t file_size = _from_data<std::uint16_t>(data + i + 1);

    i += 3;
    switch (file_id) {
      case FID_DEVICE_INFO: {
        if (from_data(info, data + i) != file_size) {
          LOGI("%s %d:: The firmware not support getting device info,"
              "you could upgrade to latest.", __FILE__, __LINE__);
          return false;
        }
        spec_ver = &info->spec_version;
        CheckSpecVersion(spec_ver);
      } break;
      case FID_RESERVE: break;
      case FID_IMU_PARAMS: {
        imu_params->ok = file_size > 0;
        if (imu_params->ok) {
          CheckSpecVersion(spec_ver);
          if (from_data(imu_params, data + i, spec_version)
              != file_size) { return false; }
        }
      } break;
      default:
        LOGI("%s %d:: Unsupported file id: %u",
            __FILE__, __LINE__, file_id);
    }
    i += file_size;
  }

  return true;
}

namespace {

template <typename T>
std::size_t _to_data(T value, std::uint8_t *data) {
  std::size_t size = sizeof(T) / sizeof(std::uint8_t);
  for (std::size_t i = 0; i < size; i++) {
    data[i] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
  }
  return size;
}

template <>
std::size_t _to_data(double value, std::uint8_t *data) {
  std::uint8_t *val = reinterpret_cast<std::uint8_t *>(&value);
  std::copy(val, val + 8, data);
  return 8;
}

std::size_t _to_data(std::string value, std::uint8_t *data, std::size_t count) {
  std::copy(value.begin(), value.end(), data);
  for (std::size_t i = value.size(); i < count; i++) {
    data[i] = ' ';
  }
  return count;
}

std::size_t to_data(
    const Channels::device_info_t *info, std::uint8_t *data,
    const Version *spec_version) {
  std::size_t i = 4;             // skip vid, pid
  // name, 20
  _to_data(info->name, data + i, 20);
  i += 20;
  // serial_number, 24
  _to_data(info->serial_number, data + i, 24);
  i += 24;
  // firmware_version, 2
  data[i] = info->firmware_version.minor();
  data[i + 1] = info->firmware_version.major();
  i += 2;
  // hardware_version, 3
  data[i] = info->hardware_version.minor();
  data[i + 1] = info->hardware_version.major();
  data[i + 2] =
      static_cast<std::uint8_t>(info->hardware_version.flag().to_ulong());
  i += 3;
  // spec_version, 2
  data[i] = info->spec_version.minor();
  data[i + 1] = info->spec_version.major();
  i += 2;
  // lens_type, 4
  _to_data(info->lens_type.vendor(), data + i);
  _to_data(info->lens_type.product(), data + i + 2);
  i += 4;
  // imu_type, 4
  _to_data(info->imu_type.vendor(), data + i);
  _to_data(info->imu_type.product(), data + i + 2);
  i += 4;
  // nominal_baseline, 2
  _to_data(info->nominal_baseline, data + i);
  i += 2;

  // others
  std::size_t size = i - 3;
  data[0] = Channels::FID_DEVICE_INFO;
  data[1] = static_cast<std::uint8_t>(size & 0xFF);
  data[2] = static_cast<std::uint8_t>((size >> 8) & 0xFF);

  UNUSED(spec_version);
  return size + 3;
}

std::size_t to_data(const ImuIntrinsics *in,
    std::uint8_t *data, const Version *spec_version) {
  std::size_t i = 0;

  // scale
  for (std::size_t j = 0; j < 3; j++) {
    for (std::size_t k = 0; k < 3; k++) {
      _to_data(in->scale[j][k], data + i + (j * 3 + k) * 8);
    }
  }
  i += 72;
  // assembly
  for (std::size_t j = 0; j < 3; j++) {
    for (std::size_t k = 0; k < 3; k++) {
      _to_data(in->assembly[j][k], data + i + (j * 3 + k) * 8);
    }
  }
  i += 72;
  // drift
  for (std::size_t j = 0; j < 3; j++) {
    _to_data(in->drift[j], data + i + j * 8);
  }
  i += 24;
  // noise
  for (std::size_t j = 0; j < 3; j++) {
    _to_data(in->noise[j], data + i + j * 8);
  }
  i += 24;
  // bias
  for (std::size_t j = 0; j < 3; j++) {
    _to_data(in->bias[j], data + i + j * 8);
  }
  i += 24;
  i += 100;
  // warm drift
  // x
  for (std::size_t j = 0; j < 2; j++) {
    _to_data<double>(in->x[j], data + i + j * 8);
  }
  i += 16;
  // y
  for (std::size_t j = 0; j < 2; j++) {
    _to_data<double>(in->y[j], data + i + j * 8);
  }
  i += 16;
  // z
  for (std::size_t j = 0; j < 2; j++) {
    _to_data<double>(in->z[j], data + i + j * 8);
  }
  i += 16;

  UNUSED(spec_version);
  return i;
}

std::size_t to_data(const Extrinsics *ex, std::uint8_t *data,
    const Version *spec_version) {
  std::size_t i = 0;

  // rotation
  for (std::size_t j = 0; j < 3; j++) {
    for (std::size_t k = 0; k < 3; k++) {
      _to_data(ex->rotation[j][k], data + i + (j * 3 + k) * 8);
    }
  }
  i += 72;
  // translation
  for (std::size_t j = 0; j < 3; j++) {
    _to_data(ex->translation[j], data + i + j * 8);
  }
  i += 24;

  UNUSED(spec_version);
  return i;
}

std::size_t to_data(
    const Channels::imu_params_t *imu_params,
    std::uint8_t *data,
    const Version *spec_version) {
  std::size_t i = 0;
  i += to_data(&imu_params->in_accel, data + i, spec_version);
  i += to_data(&imu_params->in_gyro, data + i, spec_version);
  i += to_data(&imu_params->ex_left_to_imu, data + i, spec_version);

  // others
  std::size_t size = i - 3;
  data[0] = Channels::FID_IMU_PARAMS;
  data[1] = static_cast<std::uint8_t>(size & 0xFF);
  data[2] = static_cast<std::uint8_t>((size >> 8) & 0xFF);
  return size + 3;
}

} // namespace

bool Channels::UpdateFileData(
    std::uint8_t *data, std::uint16_t size) {
  std::uint8_t cmd[64];

  cmd[0] = 0x8A;
  cmd[1] = 4;
  cmd[2] = size & 0xFF;
  cmd[3] = (size & 0xFF00) >> 8;
  cmd[4] = (size & 0xFF0000) >> 16;
  cmd[5] = (size & 0xFF000000) >> 24;

  if (device_->get_device_class() == 0xFF) {
    LOGE("%s %d:: Not support filechannel, please update firmware.",
        __FILE__, __LINE__);
    return false;
  }

  if (device_->send(0, cmd, 64, 200) <= 0) {
    LOGE("%s %d:: Error reading, device not ready, retrying",
        __FILE__, __LINE__);
    return false;
  }

  while (0x8B != cmd[0]) {
    device_->receive(0, cmd, 64, 2000);
    if (++req_count_ > 5) {
      LOGE("%s %d:: Error reading, device went offline.",
          __FILE__, __LINE__);
      return false;
    }
  }

  std::uint32_t packets_index = 0;
  std::uint8_t *seek = data;
  std::uint16_t file_size = size;
  while (true) {
    std::uint16_t current_sz = 0;
    if (file_size >= 60) {
      std::copy(seek, seek + 60, cmd + 3);
      seek += 60;
      file_size -= 60;
      current_sz = 60;
    } else {
      std::copy(seek, seek + file_size, cmd + 3);
      current_sz = file_size;
      file_size = 0;
    }

    cmd[0] = 0x5A;
    cmd[1] = packets_index;
    cmd[2] = current_sz;
    cmd[current_sz + 3] = check_sum(cmd + 3, current_sz);

    if (device_->send(0, cmd, 64, 100) <= 0) {
      LOGE("%s %d:: Update file data failure.", __FILE__, __LINE__);
      return false;
    }
    packets_index++;

    if (file_size == 0) {
      cmd[0] = 0xAA;
      cmd[1] = 0xFF;
      if (device_->send(0, cmd, 64, 100) <= 0) {
        LOGE("%s %d:: Update file data failure.", __FILE__, __LINE__);
        return false;
      } else {
        break;
      }
    }
  }

  return true;
}

bool Channels::SetFiles(device_info_t *info,
    imu_params_t *imu_params,
    Version *spec_version) {
  if (info == nullptr && imu_params == nullptr) {
    LOGE("%s %d:: Files are not provided to set.", __FILE__, __LINE__);
    return false;
  }

  Version *spec_ver = spec_version;
  if (spec_ver == nullptr && info != nullptr) {
    spec_ver = &info->spec_version;
  }
  CheckSpecVersion(spec_ver);

  std::uint8_t data[2000]{};

  std::uint16_t size = 3;
  if (info != nullptr) {
    data[0] |= 0x80 | 0x01 << 0;
    std::uint16_t data_size = to_data(info, data + size + 3, spec_ver);
    *(data + size) = 0x01 << 0;
    *(data + size + 1) = data_size & 0xFF;
    *(data + size + 2) = (data_size >> 8) & 0xFF;
    size += 3;
    size += data_size;
  }

  data[0] |= 0x80 | 0x00 << 1;

  if (imu_params != nullptr) {
    data[0] |= 0x80 | 0x01 << 2;
    std::uint16_t data_size = to_data(imu_params, data + size + 3, spec_ver);
    *(data + size) = 0x01 << 2;
    *(data + size + 1) = data_size & 0xFF;
    *(data + size + 2) = (data_size >> 8) & 0xFF;
    size += 3;
    size += data_size;
  }

  size -= 3;
  data[1] = static_cast<std::uint8_t>(size & 0xFF);
  data[2] = static_cast<std::uint8_t>((size >> 8) & 0xFF);
  data[size + 3] = check_sum(data + 3, size);
  size += 4;

  if (!UpdateFileData(data, size)) {
    LOGE("%s %d:: Update file data failure.",
        __FILE__, __LINE__);
    return false;
  }

  return true;
}

MYNTEYE_END_NAMESPACE
