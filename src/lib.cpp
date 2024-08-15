#include <napi.h>

#include <cstdint>
#include <memory>

#include "./utils.hpp"

extern "C" {
#include "../vgmstream/src/base/plugins.h"
#include "../vgmstream/src/vgmstream.h"
#include "../vgmstream/version.h"
}

static auto GetVersion(const Napi::CallbackInfo &info) {
  auto $ = Helper(info);

  return $.object([&](auto version) {
    version["version"] = VGMSTREAM_VERSION;
    version["extension"] = $.object([&](auto extension) {
      size_t extension_list_len = 0;
      const char **extension_list = nullptr;

      extension_list = vgmstream_get_formats(&extension_list_len);
      extension["vgm"] =
          $.array<const char *>(extension_list_len, [&](const size_t index) { return extension_list[index]; });

      extension_list = vgmstream_get_common_formats(&extension_list_len);
      extension["common"] =
          $.array<const char *>(extension_list_len, [&](const size_t index) { return extension_list[index]; });
    });
  });
}

static auto GetSubSongCount(const Napi::CallbackInfo &info) {
  auto $ = Helper(info);

  auto buffer = info[0];
  if (!buffer.IsBuffer()) {
    return $.throws("arg[0] in getSubSongCount not a buffer");
  }

  auto vgmstream = vgmstreamFromBuffer(buffer.As<Napi::Buffer<uint8_t>>());

  return $.number(vgmstream->num_streams);
}

auto GetMetaImpl(const Helper &$, const Napi::Buffer<uint8_t> &buffer, int stream_index) {
  auto vgmstream = vgmstreamFromBuffer(buffer.As<Napi::Buffer<uint8_t>>(), stream_index);

  vgmstream_info bank_info;
  describe_vgmstream_info(vgmstream.get(), &bank_info);

  return $.object([&](auto meta) {
    meta["version"] = VGMSTREAM_VERSION;
    meta["sampleRate"] = bank_info.sample_rate;
    meta["channels"] = bank_info.channels;

    if (bank_info.mixing_info.input_channels > 0) {
      meta["mixingInfo"] = $.object([&](auto mixing_info) {
        mixing_info["inputChannels"] = bank_info.mixing_info.input_channels;
        mixing_info["outputChannels"] = bank_info.mixing_info.output_channels;
      });
    } else {
      meta["mixingInfo"] = $.null();
    }

    if (bank_info.channel_layout == 0) {
      meta["channelLayout"] = $.null();
    } else {
      meta["channelLayout"] = bank_info.channel_layout;
    }

    if (bank_info.loop_info.end > bank_info.loop_info.start) {
      meta["loopingInfo"] = $.object([&](auto looping_info) {
        looping_info["start"] = bank_info.loop_info.start;
        looping_info["end"] = bank_info.loop_info.end;
      });
    } else {
      meta["loopingInfo"] = $.null();
    }

    if (bank_info.interleave_info.last_block > bank_info.interleave_info.first_block) {
      meta["interleaveInfo"] = $.object([&](auto interleave_info) {
        interleave_info["firstBlock"] = bank_info.interleave_info.first_block;
        interleave_info["lastBlock"] = bank_info.interleave_info.last_block;
      });
    } else {
      meta["interleaveInfo"] = $.null();
    }

    meta["numberOfSamples"] = bank_info.num_samples;
    meta["encoding"] = bank_info.encoding;
    meta["layout"] = bank_info.layout;
    meta["frameSize"] = bank_info.frame_size;
    meta["metadataSource"] = bank_info.metadata;
    meta["bitrate"] = bank_info.bitrate;

    meta["streamInfo"] = $.object([&](auto stream_info) {
      stream_info["index"] = bank_info.stream_info.current;
      stream_info["name"] = bank_info.stream_info.name;
      stream_info["total"] = bank_info.stream_info.total;
    });
  });
}

static auto GetAllMeta(const Napi::CallbackInfo &info) {
  auto $ = Helper(info);

  auto _buffer = info[0];
  if (!_buffer.IsBuffer()) {
    return $.throws("arg[0] in getAllMeta is not a buffer");
  }
  auto buffer = _buffer.As<Napi::Buffer<uint8_t>>();

  auto vgmstream = vgmstreamFromBuffer(buffer);

  auto meta_arr = $.array<Napi::Object>(vgmstream->num_streams, [&](const size_t index) {
    // NOLINTNEXTLINE bugprone-narrowing-conversions
    return GetMetaImpl($, buffer, index + 1);
  });

  return meta_arr.As<Napi::Value>();
}

static auto GetMeta(const Napi::CallbackInfo &info) {
  auto $ = Helper(info);

  auto _buffer = info[0];
  if (!_buffer.IsBuffer()) {
    return $.throws("arg[0] in getMeta is not a buffer");
  }
  auto buffer = _buffer.As<Napi::Buffer<uint8_t>>();

  auto _index = info[1];
  if (!_index.IsNumber()) {
    return $.throws("arg[1] in getMeta is not a number");
  }
  auto index = _index.As<Napi::Number>().Int32Value();

  return GetMetaImpl($, buffer, index).As<Napi::Value>();
}

static auto Init(Napi::Env env, Napi::Object exports) -> Napi::Object {
  auto property = Napi::PropertyDescriptor::Accessor("version", GetVersion, napi_default);
  exports.DefineProperty(property);
  exports.Set("getSubSongCount", Napi::Function::New(env, GetSubSongCount));
  exports.Set("getAllMeta", Napi::Function::New(env, GetAllMeta));
  exports.Set("getMeta", Napi::Function::New(env, GetMeta));
  return exports;
}

// NOLINTNEXTLINE modernize-use-trailing-return-type
NODE_API_MODULE(hello, Init)
