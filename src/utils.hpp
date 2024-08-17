#ifndef SRC_UTILS_HPP_
#define SRC_UTILS_HPP_

#include <napi.h>
#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>

extern "C" {
#include "vgmstream/src/streamfile.h"
#include "vgmstream/src/vgmstream.h"
}

using NapiBuffer = Napi::Buffer<uint8_t>;

template <typename T>
constexpr auto unsigned_multiplier() {
  // NOLINTBEGIN readability-magic-numbers
  constexpr auto multiplier = std::is_same_v<T, uint8_t>  ? 1
                            : std::is_same_v<T, uint16_t> ? 2
                            : std::is_same_v<T, uint32_t> ? 4
                            : std::is_same_v<T, uint32_t> ? 8
                                                          : 0;  // never
  // NOLINTEND readability-magic-numbers
  static_assert(multiplier != 0, "T must inherit from one of uint8_t, uint16_t, uint32_t, uint64_t");
  return multiplier;
}

class ExtendableBuffer {
 public:
  explicit ExtendableBuffer(const size_t initial_size = 1024) : initial_size(initial_size) { initialize(); }
  ~ExtendableBuffer() {
    if (initialized) {
      delete[] ptr;
    }
  }

  void initialize() {
    initialized = true;
    ptr = new uint8_t[initial_size];
    capacity = initial_size;
    current = 0;
  }

  template <typename T>
  void ensure(const size_t length) {
    if (!initialized) {
      return;
    }
    constexpr auto multiplier = unsigned_multiplier<T>();
    if (current + length * multiplier > capacity) {
      resize(std::max(capacity * 2, current + length * multiplier));
    }
  }

  template <typename T>
  void push(const T *buffer, const size_t length) {
    if (!initialized) {
      return;
    }
    auto multiplier = unsigned_multiplier<T>();
    ensure<T>(length * multiplier);
    memcpy(ptr + current, buffer, length * multiplier);
    swap<T>(length);
    current += length * multiplier;
  }

  template <typename T>
  void expose(const std::function<size_t(T *)> &callback) {
    if (!initialized) {
      return;
    }
    auto written = callback(reinterpret_cast<T *>(ptr + current));
    auto multiplier = unsigned_multiplier<T>();
    swap<T>(written);
    current += written * multiplier;
  }

  void resize(const size_t new_size) {
    if (!initialized) {
      return;
    }
    auto *new_ptr = new uint8_t[new_size];
    memcpy(new_ptr, ptr, current);
    delete[] ptr;
    ptr = new_ptr;
    capacity = new_size;
  }

  auto move_to_node_buffer(const Napi::Env &env) {
    if (!initialized) {
      auto *empty_buf = new uint8_t[1];
      return Napi::Buffer<uint8_t>::New(env, empty_buf, 0, [](auto _env, auto buf) { delete[] buf; });
    }
    initialized = false;
    auto buf = Napi::Buffer<uint8_t>::New(env, ptr, current, [](auto _env, auto buf) { delete[] buf; });
    return buf;
  }

 private:
  uint8_t *ptr;
  const size_t initial_size;
  bool initialized = false;
  size_t capacity;
  size_t current;

  template <typename T>
  void swap(const size_t count) {
#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
    auto multiplier = unsigned_multiplier<T>();
    if (multiplier == 1) {
      return;
    }
    // NOLINTBEGIN readability-magic-numbers
    for (auto *i = ptr + current; i < ptr + current + count * multiplier; i += multiplier) {
      if (std::is_same_v<T, uint16_t>) {
        std::swap(*i, *(i + 1));
      } else if (std::is_same_v<T, uint32_t>) {
        std::swap(*i, *(i + 3));
        std::swap(*(i + 1), *(i + 2));
      } else if (std::is_same_v<T, uint64_t>) {
        std::swap(*i, *(i + 7));
        std::swap(*(i + 1), *(i + 6));
        std::swap(*(i + 2), *(i + 5));
        std::swap(*(i + 3), *(i + 4));
      }
    }
// NOLINTEND readability-magic-numbers
#endif
  }
};

class Helper {
 public:
  Napi::Env env = nullptr;
  explicit Helper(const Napi::Env &env) : env(env) {}

  auto throws(const char *error_message) const {
    Napi::Error::New(this->env, error_message).ThrowAsJavaScriptException();
    return this->env.Undefined();
  }

  [[nodiscard]]
  auto null() const {
    return this->env.Null();
  }

  [[nodiscard]]
  auto undefined() const {
    return this->env.Undefined();
  }

  template <typename T>
  [[nodiscard]]
  auto number(const T &number) const {
    return Napi::Number::From(this->env, number);
  }

  [[nodiscard]]
  auto object(const std::function<void(Napi::Object &)> &initializer) const {
    auto obj = Napi::Object::New(this->env);
    initializer(obj);
    return obj;
  }

  template <typename T>
  [[nodiscard]]
  auto array(const size_t length, const std::function<T(const size_t index)> &item_generator) const {
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
  explicit NodeBufferStreamFile(const NapiBuffer &buffer, const int stream_index)
      : buffer(buffer), vt(new_inner_streamfile(stream_index)) {}

  STREAMFILE vt;
  std::string filename = "whatever.bank";
  NapiBuffer buffer;
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

auto vgmstreamFromBuffer(const NapiBuffer &buffer, const int stream_index = 1) {
  auto *buffer_stream_file = new NodeBufferStreamFile(buffer, stream_index);
  auto *vgmstream = init_vgmstream_from_STREAMFILE(reinterpret_cast<STREAMFILE *>(buffer_stream_file));
  return std::shared_ptr<VGMSTREAM>(vgmstream, [=](auto ptr) {
    close_vgmstream(ptr);
    delete buffer_stream_file;
  });
}

#endif  // SRC_UTILS_HPP_
