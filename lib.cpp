#include <napi.h>

extern "C" {
#include "vgmstream/src/base/plugins.h"
#include "vgmstream/version.h"
}

static auto Method(const Napi::CallbackInfo &info) {
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

static auto Init(Napi::Env env, Napi::Object exports) -> Napi::Object {
  exports.Set(Napi::String::New(env, "version"),
              Napi::Function::New(env, Method));
  return exports;
}

NODE_API_MODULE(hello, Init) // NOLINT
