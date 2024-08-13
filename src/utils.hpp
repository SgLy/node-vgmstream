#ifndef SRC_UTILS_HPP_
#define SRC_UTILS_HPP_

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <napi.h>

#include <cstdint>
#include <cstdlib>
#include <string>

extern "C" {
#include "../vgmstream/src/streamfile.h"
}

class NodeBufferStreamFile {
public:
  explicit NodeBufferStreamFile(const Napi::Buffer<uint8_t> &buffer)
      : buffer(buffer) {}

  STREAMFILE vt;
  std::string filename = "whatever.bank";
  Napi::Buffer<uint8_t> buffer;
  offv_t offset;
};

/* get max offset */
size_t read(NodeBufferStreamFile *stream_file, uint8_t *dst, offv_t offset,
            size_t length) {
  memcpy(dst, stream_file->buffer.Data() + offset, length);
  stream_file->offset = offset + length;
  return length;
}

/* get max offset */
size_t get_size(NodeBufferStreamFile *stream_file) {
  return stream_file->buffer.Length();
}

// todo: DO NOT USE, NOT RESET PROPERLY (remove?)
offv_t get_offset(NodeBufferStreamFile *stream_file) {
  return stream_file->offset;
}

/* copy current filename to name buf */
void get_name(NodeBufferStreamFile *stream_file, char *name, size_t name_size) {
  auto size = std::min(stream_file->filename.size() + 1, name_size);
  memcpy(name, stream_file->filename.c_str(), size);
  name[size - 1] = '\0';
}

/* open another streamfile from filename */
STREAMFILE *open(NodeBufferStreamFile *stream_file, const char *const filename,
                 size_t buf_size) {
  if (strcmp(filename, stream_file->filename.c_str()) == 0) {
    return &stream_file->vt;
  }
  return nullptr;
}

/* free current STREAMFILE */
void close(NodeBufferStreamFile *stream_file) {
  // do nothing
}

auto StreamFileFromBuffer(const Napi::CallbackInfo &info,
                          const Napi::Buffer<uint8_t> &buffer) {
  auto buffer_stream_file = NodeBufferStreamFile(buffer);
  auto stream_file = STREAMFILE{
      .read = reinterpret_cast<size_t (*)(struct _STREAMFILE *, uint8_t *,
                                          offv_t, size_t)>(read),
      .get_size = reinterpret_cast<size_t (*)(struct _STREAMFILE *)>(get_size),
      .get_offset =
          reinterpret_cast<offv_t (*)(struct _STREAMFILE *)>(get_offset),
      .get_name =
          reinterpret_cast<void (*)(struct _STREAMFILE *, char *, size_t)>(
              get_name),
      .open =
          reinterpret_cast<struct _STREAMFILE *(*)(struct _STREAMFILE *,
                                                   const char *const, size_t)>(
              open),
      .close = reinterpret_cast<void (*)(struct _STREAMFILE *)>(close),
      .stream_index = 0,
  };
  buffer_stream_file.vt = stream_file;
  return buffer_stream_file;
}

#endif  // SRC_UTILS_HPP_
