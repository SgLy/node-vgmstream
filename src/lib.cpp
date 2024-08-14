#include <napi.h>

#include "./utils.hpp"

extern "C" {
#include "../vgmstream/src/base/plugins.h"
#include "../vgmstream/src/vgmstream.h"
#include "../vgmstream/version.h"
}

static auto GetVersion(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  auto helper = Helper(env);

  return helper.object([&](auto version) {
    version["version"] = VGMSTREAM_VERSION;
    version["extension"] = helper.object([&](auto extension) {
      size_t extension_list_len = 0;
      const char **extension_list = nullptr;

      extension_list = vgmstream_get_formats(&extension_list_len);
      extension["vgm"] = helper.array<const char *>(
          extension_list_len,
          [&](const size_t index) { return extension_list[index]; });

      extension_list = vgmstream_get_common_formats(&extension_list_len);
      extension["common"] = helper.array<const char *>(
          extension_list_len,
          [&](const size_t index) { return extension_list[index]; });
    });
  });
}

static auto GetMeta(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  auto helper = Helper(env);

  auto buffer = info[0];
  if (!buffer.IsBuffer()) {
    return helper.throws("not a buffer");
  }

  auto stream_file =
      StreamFileFromBuffer(info, buffer.As<Napi::Buffer<uint8_t>>());

  auto *vgmstream = init_vgmstream_from_STREAMFILE(&stream_file.vt);
  auto total = vgmstream->num_streams;
  close_vgmstream(vgmstream);

  auto meta_arr = helper.array<Napi::Object>(total, [&](const size_t index) {
    stream_file.vt.stream_index = index + 1; // NOLINT

    auto *vgmstream = init_vgmstream_from_STREAMFILE(&stream_file.vt);

    vgmstream_info bank_info;
    describe_vgmstream_info(vgmstream, &bank_info);

    auto meta = helper.object([&](auto meta) {
      meta["version"] = VGMSTREAM_VERSION;
      meta["sampleRate"] = bank_info.sample_rate;
      meta["channels"] = bank_info.channels;

      if (bank_info.mixing_info.input_channels > 0) {
        meta["mixingInfo"] = helper.object([&](auto mixing_info) {
          mixing_info["inputChannels"] = bank_info.mixing_info.input_channels;
          mixing_info["outputChannels"] = bank_info.mixing_info.output_channels;
        });
      } else {
        meta["mixingInfo"] = env.Null();
      }

      if (bank_info.channel_layout == 0) {
        meta["channelLayout"] = env.Null();
      } else {
        meta["channelLayout"] = bank_info.channel_layout;
      }

      if (bank_info.loop_info.end > bank_info.loop_info.start) {
        meta["loopingInfo"] = helper.object([&](auto looping_info) {
          looping_info["start"] = bank_info.loop_info.start;
          looping_info["end"] = bank_info.loop_info.end;
        });
      } else {
        meta["loopingInfo"] = env.Null();
      }

      if (bank_info.interleave_info.last_block >
          bank_info.interleave_info.first_block) {
        meta["interleaveInfo"] = helper.object([&](auto interleave_info) {
          interleave_info["firstBlock"] = bank_info.interleave_info.first_block;
          interleave_info["lastBlock"] = bank_info.interleave_info.last_block;
        });
      } else {
        meta["interleaveInfo"] = env.Null();
      }

      meta["numberOfSamples"] = bank_info.num_samples;
      meta["encoding"] = bank_info.encoding;
      meta["layout"] = bank_info.layout;
      meta["frameSize"] = bank_info.frame_size;
      meta["metadataSource"] = bank_info.metadata;
      meta["bitrate"] = bank_info.bitrate;

      meta["streamInfo"] = helper.object([&](auto stream_info) {
        stream_info["index"] = bank_info.stream_info.current;
        stream_info["name"] = bank_info.stream_info.name;
        stream_info["total"] = bank_info.stream_info.total;
      });
    });

    close_vgmstream(vgmstream);

    return meta;
  });

  return meta_arr.As<Napi::Value>();
}

static auto Init(Napi::Env env, Napi::Object exports) -> Napi::Object {
  auto property =
      Napi::PropertyDescriptor::Accessor("version", GetVersion, napi_default);
  exports.DefineProperty(property);
  exports.Set("getMeta", Napi::Function::New(env, GetMeta));
  return exports;
}

NODE_API_MODULE(hello, Init) // NOLINT
