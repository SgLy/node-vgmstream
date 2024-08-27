#ifndef SRC_UTILS_HPP_
#define SRC_UTILS_HPP_

#include <napi.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>

extern "C" {
#include "vgmstream/src/streamfile.h"
#include "vgmstream/src/vgmstream.h"
}

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#define SWAP_REQUIRED true
#else
#define SWAP_REQUIRED false
#endif

using NapiBuffer = Napi::Buffer<uint8_t>;

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
    if (current + length * sizeof(T) > capacity) {
      resize(std::max(capacity * 2, current + length * sizeof(T)));
    }
  }

  template <typename T>
  void push(const T *buffer, const size_t length) {
    if (!initialized) {
      return;
    }
    ensure<T>(length * sizeof(T));
    memcpy(ptr + current, buffer, length * sizeof(T));
    swap<T>(length);
    current += length * sizeof(T);
  }

  template <typename T>
  void expose(const std::function<size_t(T *)> &callback) {
    if (!initialized) {
      return;
    }
    auto written = callback(reinterpret_cast<T *>(ptr + current));
    swap<T>(written);
    current += written * sizeof(T);
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
      return Napi::Buffer<uint8_t>::New(env, empty_buf, 0, [](auto env, auto buf) { delete[] buf; });
    }
    initialized = false;
    auto buf = Napi::Buffer<uint8_t>::New(env, ptr, current, [](auto env, auto buf) { delete[] buf; });
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
#if SWAP_REQUIRED
    if (sizeof(T) == 1) {
      return;
    }
    for (auto *i = ptr + current; i < ptr + current + count * sizeof(T); i += sizeof(T)) {
      for (auto *left_ptr = i, *right_ptr = i + sizeof(T) - 1; left_ptr < right_ptr; ++left_ptr, --right_ptr) {
        std::swap(*left_ptr, *right_ptr);
      }
    }
#endif
  }
};

template <typename T>
class Promise {
 public:
  using ResolveFunc = std::function<void(T *)>;
  using RejectFunc = std::function<void(const char *)>;
  using PromiseFunc = std::function<void(ResolveFunc, RejectFunc)>;
  using TransformFunc = std::function<Napi::Value(Napi::Env env, T *value)>;

 private:
  using Data = std::tuple<bool, T *, const char *>;
  using FinalizerDataType = void;

  static void call_js(Napi::Env env, Napi::Function callback, Promise *context, Data *data) {
    auto [is_success, value, error_message] = *data;
    context->thread.join();
    if (is_success) {
      context->deferred.Resolve(context->transform(env, value));
    } else {
      context->deferred.Reject(Napi::String::From(env, error_message));
    }
    delete value;
    delete data;
  }

  using ThreadSafeFunction = Napi::TypedThreadSafeFunction<Promise, Data, Promise::call_js>;

 public:
  explicit Promise(Napi::Env env, PromiseFunc &&process, TransformFunc &&transform)
      : deferred(Napi::Promise::Deferred::New(env)), transform(std::move(transform)) {
    this->func =
        ThreadSafeFunction::New(env, "Resource Name", 0, 1, this, [](Napi::Env, FinalizerDataType *, Promise *context) {
          delete context;
        });
    this->thread = std::thread([process = std::move(process), this] {
      bool resolved = false;
      bool rejected = false;
      T *resolved_value = nullptr;
      const char *error_message = nullptr;
      process(
          [&](auto value) {
            resolved = true;
            resolved_value = value;
          },
          [&](auto message) {
            rejected = true;
            error_message = message;
          }
      );
      if (resolved) {
        this->func.BlockingCall(new Data(true, resolved_value, nullptr));
      } else if (rejected) {
        this->func.BlockingCall(new Data(false, nullptr, error_message));
      } else {
        throw std::logic_error("Native error: Promise should either resolved or rejected.");
      }
      this->func.Release();
    });
  }

  [[nodiscard]]
  auto promise() const -> Napi::Promise {
    return this->deferred.Promise();
  }

 private:
  Napi::Promise::Deferred deferred;
  std::thread thread;
  ThreadSafeFunction func;
  TransformFunc transform;
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

  template <typename T>
  [[nodiscard]]
  auto string(const T &string) const {
    return Napi::String::From(this->env, string);
  }

  [[nodiscard]]
  auto object(const std::function<void(Napi::Object &)> &initializer) const {
    auto obj = Napi::Object::New(this->env);
    initializer(obj);
    return obj;
  }

  using NapiGetter = std::function<Napi::Value(const Napi::CallbackInfo &)>;

  [[nodiscard]]
  auto object(const std::function<void(Napi::Object &, const std::function<void(const char *, const NapiGetter &)> &)>
                  &initializer) const {
    auto obj = Napi::Object::New(this->env);
    initializer(obj, [&](auto *name, auto getter) {
      auto descriptor = Napi::PropertyDescriptor::Accessor(env, obj, name, getter);
      obj.DefineProperty(descriptor);
    });
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

  template <typename T>
  [[nodiscard]]
  auto async(typename Promise<T>::PromiseFunc &&process, typename Promise<T>::TransformFunc &&transform) const {
    auto promise = new Promise<T>(this->env, std::move(process), std::move(transform));
    return promise->promise();
  }
};

template <typename T>
auto obtain_arg(const Napi::CallbackInfo &info, const size_t index) -> T {
  auto arg = info[index];
  auto is_correct = false;
  if (std::is_same_v<T, Napi::Number>) {
    is_correct = arg.IsNumber();
  } else if (std::is_same_v<T, Napi::String>) {
    is_correct = arg.IsString();
  } else if (std::is_same_v<T, NapiBuffer>) {
    is_correct = arg.IsBuffer();
  }
  constexpr auto typestr = std::is_same_v<T, Napi::Number> ? "number"
                         : std::is_same_v<T, Napi::String> ? "string"
                         : std::is_same_v<T, NapiBuffer>   ? "buffer"
                                                           : "unknown";
  if (!is_correct) {
    auto length = snprintf(nullptr, 0, "expect arg[%lu] to be a %s but mismatched", index, typestr);
    auto *buf = new char[length + 1];
    snprintf(buf, length + 1, "expect arg[%lu] to be a %s but not", index, typestr);
    Napi::Error::New(info.Env(), buf).ThrowAsJavaScriptException();
    delete[] buf;
  }
  return arg.As<T>();
}

template <typename T>
auto obtain_arg(const Napi::CallbackInfo &info, const size_t index, const T &default_value) -> T {
  return index < info.Length() ? obtain_arg<T>(info, index) : default_value;
}

auto new_inner_streamfile(int stream_index) -> STREAMFILE;

class NodeBufferStreamFile {
 public:
  explicit NodeBufferStreamFile(const NapiBuffer &buffer, const int stream_index, std::string filename)
      : buffer(buffer.Data()),
        length(buffer.Length()),
        vt(new_inner_streamfile(stream_index)),
        filename(std::move(filename)) {}
  ~NodeBufferStreamFile() {}

  STREAMFILE vt;
  std::string filename;
  uint8_t const *buffer;
  size_t length;
  offv_t offset;
};

/* get max offset */
auto read(NodeBufferStreamFile *stream_file, uint8_t *dst, offv_t offset, size_t length) -> size_t {
  memcpy(dst, stream_file->buffer + offset, std::min(length, stream_file->length - offset));
  // NOLINTNEXTLINE bugprone-narrowing-conversions
  stream_file->offset = offset + length;
  return length;
}

/* get max offset */
auto get_size(NodeBufferStreamFile *stream_file) -> size_t { return stream_file->length; }

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

auto vgmstream_from_buffer(const NapiBuffer &buffer, const int stream_index, const std::string &filename) {
  auto *buffer_stream_file = new NodeBufferStreamFile(buffer, stream_index, filename);
  auto *vgmstream = init_vgmstream_from_STREAMFILE(reinterpret_cast<STREAMFILE *>(buffer_stream_file));
  return std::shared_ptr<VGMSTREAM>(vgmstream, [=](auto ptr) {
    close_vgmstream(ptr);
    delete buffer_stream_file;
  });
}

#endif  // SRC_UTILS_HPP_
