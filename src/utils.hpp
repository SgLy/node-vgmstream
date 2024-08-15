#ifndef SRC_UTILS_HPP_
#define SRC_UTILS_HPP_

#include <napi.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

extern "C" {
#include "../vgmstream/src/streamfile.h"
#include "../vgmstream/src/vgmstream.h"
}

class Helper {
 public:
  const Napi::CallbackInfo *info = nullptr;
  Napi::Env env = nullptr;
  explicit Helper(const Napi::CallbackInfo &info) {
    this->info = &info;
    this->env = this->info->Env();
  }

  auto throws(const char *error_message) const {
    Napi::Error::New(this->env, error_message).ThrowAsJavaScriptException();
    return this->env.Undefined();
  }

  [[nodiscard]] auto null() const { return this->env.Null(); }

  [[nodiscard]] auto undefined() const { return this->env.Undefined(); }

  template <typename T>
  [[nodiscard]] auto number(const T &number) const {
    return Napi::Number::From(this->env, number);
  }

  [[nodiscard]] auto object(const std::function<void(Napi::Object &)> &initializer) const {
    auto obj = Napi::Object::New(this->env);
    initializer(obj);
    return obj;
  }

  template <typename T>
  [[nodiscard]] auto array(const size_t length, const std::function<T(const size_t index)> &item_generator) const {
    auto arr = Napi::Array::New(this->env, length);
    for (size_t i = 0; i < length; ++i) {
      arr[i] = item_generator(i);
    }
    return arr;
  }
};

auto new_inner_streamfile(int stream_index) -> STREAMFILE;

class NodeBufferStreamFile {
 public:
  explicit NodeBufferStreamFile(const Napi::Buffer<uint8_t> &buffer, const int stream_index)
      : buffer(buffer), vt(new_inner_streamfile(stream_index)) {}

  STREAMFILE vt;
  std::string filename = "whatever.bank";
  Napi::Buffer<uint8_t> buffer;
  offv_t offset;
};

/* get max offset */
auto read(NodeBufferStreamFile *stream_file, uint8_t *dst, offv_t offset, size_t length) -> size_t {
  memcpy(dst, stream_file->buffer.Data() + offset, length);
  // NOLINTNEXTLINE bugprone-narrowing-conversions
  stream_file->offset = offset + length;
  return length;
}

/* get max offset */
auto get_size(NodeBufferStreamFile *stream_file) -> size_t { return stream_file->buffer.Length(); }

// todo: DO NOT USE, NOT RESET PROPERLY (remove?)
auto get_offset(NodeBufferStreamFile *stream_file) -> offv_t { return stream_file->offset; }

/* copy current filename to name buf */
void get_name(NodeBufferStreamFile *stream_file, char *name, size_t name_size) {
  auto size = std::min(stream_file->filename.size() + 1, name_size);
  memcpy(name, stream_file->filename.c_str(), size);
  name[size - 1] = '\0';
}
/* open another streamfile from filename */
auto open(NodeBufferStreamFile *stream_file, const char *const filename, size_t buf_size) -> STREAMFILE * {
  if (strcmp(filename, stream_file->filename.c_str()) == 0) {
    return &stream_file->vt;
  }
  return nullptr;
}

/* free current STREAMFILE */
void close(NodeBufferStreamFile *stream_file) {
  // do nothing
}

auto new_inner_streamfile(const int stream_index) -> STREAMFILE {
  return STREAMFILE{
      .read = reinterpret_cast<size_t (*)(struct _STREAMFILE *, uint8_t *, offv_t, size_t)>(read),
      .get_size = reinterpret_cast<size_t (*)(struct _STREAMFILE *)>(get_size),
      .get_offset = reinterpret_cast<offv_t (*)(struct _STREAMFILE *)>(get_offset),
      .get_name = reinterpret_cast<void (*)(struct _STREAMFILE *, char *, size_t)>(get_name),
      .open = reinterpret_cast<struct _STREAMFILE *(*)(struct _STREAMFILE *, const char *const, size_t)>(open),
      .close = reinterpret_cast<void (*)(struct _STREAMFILE *)>(close),
      .stream_index = stream_index,
  };
}

auto vgmstreamFromBuffer(const Napi::Buffer<uint8_t> &buffer, const int stream_index = 1) {
  auto *buffer_stream_file = new NodeBufferStreamFile(buffer, stream_index);
  auto *vgmstream = init_vgmstream_from_STREAMFILE(reinterpret_cast<STREAMFILE *>(buffer_stream_file));
  return std::shared_ptr<VGMSTREAM>(vgmstream, [=](auto ptr) {
    close_vgmstream(ptr);
    delete buffer_stream_file;
  });
}

#endif  // SRC_UTILS_HPP_
