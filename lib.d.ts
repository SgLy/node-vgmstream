declare module 'node-vgmstream' {
  export const version: any
  export const getSubSongCount: (buffer: Buffer) => number
  export const getMeta: (buffer: Buffer, index: number) => any
  export const getAllMeta: (buffer: Buffer) => any
}
