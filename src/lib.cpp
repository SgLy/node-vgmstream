#include <napi.h>

#include <cstdlib>
#include <cstring>
#include <memory>

#include "./utils.hpp"

extern "C" {
#include "vgmstream/cli/wav_utils.h"
#include "vgmstream/src/base/plugins.h"
#include "vgmstream/src/streamtypes.h"
#include "vgmstream/src/vgmstream.h"
#include "vgmstream/version.h"
}

class VGMStreamSubSong : public Napi::ObjectWrap<VGMStreamSubSong> {
 private:
  const Napi::CallbackInfo *info = nullptr;
  Helper $;

  using BufferRef = Napi::Reference<NapiBuffer>;

  int stream_index;
  std::shared_ptr<BufferRef> buffer_ref;
  std::string filename;
  std::shared_ptr<VGMSTREAM> vgmstream_ptr;
  vgmstream_info bank_info;

 public:
  explicit VGMStreamSubSong(const Napi::CallbackInfo &info)
      : Napi::ObjectWrap<VGMStreamSubSong>(info), info(&info), $(info.Env()) {
    auto buffer = obtain_arg<NapiBuffer>(info, 0);
    auto stream_index = obtain_arg<Napi::Number>(info, 1).Int32Value();
    auto filename = obtain_arg<Napi::String>(info, 2).Utf8Value();

    this->buffer_ref = std::make_shared<BufferRef>(std::move(BufferRef::New(buffer, 1)));
    this->vgmstream_ptr = vgmstream_from_buffer(buffer, stream_index, filename);
    describe_vgmstream_info(vgmstream_ptr.get(), &bank_info);
  }

  auto get_info(const Napi::CallbackInfo &info) -> Napi::Value {
    return $.object([&](auto meta) {
      meta["version"] = VGMSTREAM_VERSION;
      meta["sampleRate"] = bank_info.sample_rate;
      meta["channels"] = bank_info.channels;

      if (bank_info.mixing_info.input_channels > 0) {
        meta["mixingInfo"] = $.object([&](auto mixing_info) {
          mixing_info["inputChannels"] = bank_info.mixing_info.input_channels;
          mixing_info["outputChannels"] = bank_info.mixing_info.output_channels;
        });
      }

      if (bank_info.channel_layout != 0) {
        meta["channelLayout"] = bank_info.channel_layout;
      }

      if (bank_info.loop_info.end > bank_info.loop_info.start) {
        meta["loopingInfo"] = $.object([&](auto looping_info) {
          looping_info["start"] = bank_info.loop_info.start;
          looping_info["end"] = bank_info.loop_info.end;
        });
      }

      if (bank_info.interleave_info.last_block > bank_info.interleave_info.first_block) {
        meta["interleaveInfo"] = $.object([&](auto interleave_info) {
          interleave_info["firstBlock"] = bank_info.interleave_info.first_block;
          interleave_info["lastBlock"] = bank_info.interleave_info.last_block;
        });
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

  static auto render_to_wave(const std::shared_ptr<VGMSTREAM> &vgmstream_ptr) {
    // fake configs
    const size_t sample_buffer_size = 8192;
    const int32_t seek_samples2 = -1;
    const int32_t seek_samples1 = -1;

    const bool decode_only = false;

    const bool write_lwav = false;
    const int lwav_loop_start = 0;
    const int lwav_loop_end = 0;
    // end of fake configs

    auto *vgmstream = vgmstream_ptr.get();

    auto channels = vgmstream->channels;
    auto input_channels = vgmstream->channels;
    vgmstream_mixing_enable(vgmstream, 0, &input_channels, &channels);

    auto *buf = new ExtendableBuffer(sample_buffer_size * sizeof(sample_t) * input_channels);

    /* simulate seek */
    auto len_samples = vgmstream_get_samples(vgmstream);
    if (seek_samples2 >= 0) {
      len_samples -= seek_samples2;
    } else if (seek_samples1 >= 0) {
      len_samples -= seek_samples1;
    }

    if (seek_samples1 >= 0) {
      seek_vgmstream(vgmstream, seek_samples1);
    }
    if (seek_samples2 >= 0) {
      seek_vgmstream(vgmstream, seek_samples2);
    }

    const size_t buffer_size = 1024;

    /* slap on a .wav header */
    if (!decode_only) {
      buf->ensure<uint8_t>(buffer_size);
      buf->expose<uint8_t>([&](auto *wav_buf) {
        wav_header_t wav = {
            .sample_count = len_samples,
            .sample_rate = vgmstream->sample_rate,
            .channels = channels,
            .write_smpl_chunk = write_lwav,
            .loop_start = lwav_loop_start,
            .loop_end = lwav_loop_end
        };

        size_t bytes_done = wav_make_header(wav_buf, buffer_size, &wav);

        return bytes_done;
      });
    }

    /* decode */

    for (auto i = 0; i < len_samples; i += sample_buffer_size) {
      int to_get = sample_buffer_size;
      if (i + sample_buffer_size > len_samples) {
        to_get = len_samples - i;
      }

      const auto length = channels * to_get;
      buf->ensure<uint16_t>(length);
      buf->expose<uint16_t>([&](auto *buffer) {
        auto samples_done = render_vgmstream(reinterpret_cast<sample_t *>(buffer), to_get, vgmstream);

        if (!decode_only) {
          wav_swap_samples_le(buffer, length, 0);
          return length;
        }

        return 0;
      });
    }

    return buf;
  }

  auto render_sync(const Napi::CallbackInfo &info) -> Napi::Value {
    auto *buf = VGMStreamSubSong::render_to_wave(this->vgmstream_ptr);
    auto result = buf->move_to_node_buffer($.env);
    delete buf;
    return result;
  }

  auto render_async(const Napi::CallbackInfo &info) -> Napi::Value {
    auto promise = $.async<ExtendableBuffer>(
        [vgmstream_ptr = vgmstream_ptr](auto resolve, auto reject) {
          resolve(VGMStreamSubSong::render_to_wave(vgmstream_ptr));
        },
        [](auto env, auto value) { return value->move_to_node_buffer(env); }
    );
    this->buffer_ref->Ref();

    auto finalizer = [buffer_ref = buffer_ref](Napi::Env env, void *data) { buffer_ref->Unref(); };
    promise.AddFinalizer<decltype(finalizer), void>(finalizer, nullptr);

    return promise;
  }

  static auto init(Napi::Env env) {
    auto sub_song_class = DefineClass(
        env,
        "VGMStreamSubSong",
        {
            InstanceAccessor<&VGMStreamSubSong::get_info>("info"),
            InstanceMethod<&VGMStreamSubSong::render_sync>("renderSync"),
            InstanceMethod<&VGMStreamSubSong::render_async>("render"),
        }
    );
    auto *constructor = new Napi::FunctionReference();
    VGMStreamSubSong::constructor = new Napi::FunctionReference();
    *VGMStreamSubSong::constructor = Napi::Persistent(sub_song_class);
  }

  static Napi::FunctionReference *constructor;
};

Napi::FunctionReference *VGMStreamSubSong::constructor = nullptr;

class VGMStream : public Napi::ObjectWrap<VGMStream> {
 private:
  const Napi::CallbackInfo *info = nullptr;
  Helper $;

  Napi::Reference<NapiBuffer> buffer_ref;
  std::string filename;

 public:
  explicit VGMStream(const Napi::CallbackInfo &info) : Napi::ObjectWrap<VGMStream>(info), info(&info), $(info.Env()) {
    auto buffer = obtain_arg<NapiBuffer>(info, 0);
    auto filename = obtain_arg<Napi::String>(info, 1, $.string("default.bank"));
    this->filename = filename.Utf8Value();
    this->buffer_ref = Napi::Reference<NapiBuffer>::New(buffer, 1);
  }

  static auto get_version(const Napi::CallbackInfo &info) -> Napi::Value {
    auto $ = Helper(info.Env());

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

  auto get_sub_song_count(const Napi::CallbackInfo &info) -> Napi::Value {
    auto vgmstream = vgmstream_from_buffer(this->buffer_ref.Value(), 1, this->filename);

    return $.number(vgmstream->num_streams);
  }

  auto select_sub_song(const Napi::CallbackInfo &info) -> Napi::Value {
    auto stream_index = info[0];

    return VGMStreamSubSong::constructor->New({this->buffer_ref.Value(), stream_index, $.string(filename)});
  }

  static auto init(Napi::Env env, Napi::Object exports) {
    auto vgmstream_class = DefineClass(
        env,
        "VGMStream",
        {
            StaticAccessor<&VGMStream::get_version>("version"),
            InstanceAccessor<&VGMStream::get_sub_song_count>("subSongCount"),
            InstanceMethod<&VGMStream::select_sub_song>("selectSubSong"),
        }
    );
    auto *constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(vgmstream_class);
    exports["VGMStream"] = vgmstream_class;
  }
};

static auto Init(Napi::Env env, Napi::Object exports) -> Napi::Object {
  VGMStream::init(env, exports);
  VGMStreamSubSong::init(env);
  return exports;
}

// NOLINTNEXTLINE modernize-use-trailing-return-type
NODE_API_MODULE("", Init)
