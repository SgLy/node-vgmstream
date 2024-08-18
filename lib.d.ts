declare module 'node-vgmstream' {
  interface VGMStreamVersion {
    version: string;
    extension: {
      vgm: string[];
      common: string[];
    };
  }
  class VGMStream {
    static get version(): VGMStreamVersion;

    constructor(buffer: Buffer);

    get subSongCount(): number;
    selectSubSong(): VGMStreamSubSong;
  }
  interface VGMStreamSubSongInfo {
    version: string;
    sampleRate: number;
    channels: number;
    mixingInfo?: {
      inputChannels: number;
      outputChannels: number;
    };
    channelLayout?: number;
    loopingInfo?: {
      start: number;
      end: number;
    };
    interleaveInfo?: {
      firstBlock: number;
      lastBlock: number;
    };
    numberOfSamples: number;
    encoding: string;
    layout: string;
    frameSize: number;
    metadataSource: string;
    bitrate: number;
    streamInfo: {
      index: number;
      name: string;
      total: number;
    };
  }
  class VGMStreamSubSong {
    get info(): VGMStreamSubSongInfo;
    renderToWave(): Buffer;
  }
  export { VGMStream };
}
