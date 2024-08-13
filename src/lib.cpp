#include <napi.h>

#include "./utils.hpp"

extern "C" {
#include "../vgmstream/src/base/plugins.h"
#include "../vgmstream/src/vgmstream.h"
#include "../vgmstream/version.h"
}

static auto GetVersion(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  auto version = Napi::Object::New(env);
  version["version"] = VGMSTREAM_VERSION;
  {
    auto extension = Napi::Object::New(env);
    {
      size_t extension_list_len = 0;
      auto *extension_list = vgmstream_get_formats(&extension_list_len);
      auto vgm = Napi::Array::New(env, extension_list_len);
      for (int i = 0; i < extension_list_len; i++) {
        vgm[i] = extension_list[i];
      }
      extension["vgm"] = vgm;
    }
    {
      size_t extension_list_len = 0;
      auto *extension_list = vgmstream_get_common_formats(&extension_list_len);
      auto common = Napi::Array::New(env, extension_list_len);
      for (int i = 0; i < extension_list_len; i++) {
        common[i] = extension_list[i];
      }
      extension["common"] = common;
    }
    version["extension"] = extension;
  }

  return version;
}

static auto GetMeta(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();
  auto buffer = info[0];
  if (!buffer.IsBuffer()) {
    Napi::Error::New(env, "not a buffer").ThrowAsJavaScriptException();
    return env.Undefined();
  }

  auto stream_file =
      StreamFileFromBuffer(info, buffer.As<Napi::Buffer<uint8_t>>());

  auto *vgmstream = init_vgmstream_from_STREAMFILE(&stream_file.vt);
  auto total = vgmstream->num_streams;
  close_vgmstream(vgmstream);

  auto meta_arr = Napi::Array::New(env, total);
  for (auto i = 0; i < total; ++i) {
    stream_file.vt.stream_index = i + 1;
    auto *vgmstream = init_vgmstream_from_STREAMFILE(&stream_file.vt);
    vgmstream_info bank_info;
    describe_vgmstream_info(vgmstream, &bank_info);

    auto meta = Napi::Object::New(env);
    meta["version"] = VGMSTREAM_VERSION;
    meta["sampleRate"] = bank_info.sample_rate;
    meta["channels"] = bank_info.channels;

    if (bank_info.mixing_info.input_channels > 0) {
      auto mixing_info = Napi::Object::New(env);
      mixing_info["inputChannels"] = bank_info.mixing_info.input_channels;
      mixing_info["outputChannels"] = bank_info.mixing_info.output_channels;
      meta["mixingInfo"] = mixing_info;
    } else {
      meta["mixingInfo"] = env.Null();
    }

    if (bank_info.channel_layout == 0) {
      meta["channelLayout"] = env.Null();
    } else {
      meta["channelLayout"] = bank_info.channel_layout;
    }

    if (bank_info.loop_info.end > bank_info.loop_info.start) {
      auto looping_info = Napi::Object::New(env);
      looping_info["start"] = bank_info.loop_info.start;
      looping_info["end"] = bank_info.loop_info.end;
      meta["loopingInfo"] = looping_info;
    } else {
      meta["loopingInfo"] = env.Null();
    }

    meta["numberOfSamples"] = bank_info.num_samples;
    meta["encoding"] = bank_info.encoding;
    meta["layout"] = bank_info.layout;
    meta["frameSize"] = bank_info.frame_size;
    meta["metadataSource"] = bank_info.metadata;
    meta["bitrate"] = bank_info.bitrate;

    {
      auto stream_info = Napi::Object::New(env);
      stream_info["index"] = bank_info.stream_info.current;
      stream_info["name"] = bank_info.stream_info.name;
      stream_info["total"] = bank_info.stream_info.total;
      meta["streamInfo"] = stream_info;
    }

    meta_arr.Set(i, meta);

    close_vgmstream(vgmstream);
  }

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
