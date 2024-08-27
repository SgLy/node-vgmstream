/** @type {import('node-vgmstream')} */
const lib = require('./lib')

const { VGMStream } = lib

const fs = require('fs')
const path = require('path')

console.log(VGMStream.version);

const buffer = fs.readFileSync(path.join(__dirname, 'test.bank'))

console.log('source buffer size: ', buffer.length)

const vgmstream = new VGMStream(buffer, 'test.bank');

console.log('total sub songs: ', vgmstream.subSongCount);

const subSong = vgmstream.selectSubSong(1)

console.log(subSong.info)

const timing = key => cb => {
  const begin = Date.now()
  cb(() => {
    console.log(`[${key}] cost: ${Date.now() - begin}ms`)
  })
}

timing('sync')(finish => {
  const lengths = [];

  for (let i = 0; i < vgmstream.subSongCount; ++i) {
    const subSong = vgmstream.selectSubSong(i);
    lengths.push(subSong.renderSync().length);
  }

  console.log(lengths)

  finish();
})

timing('async')(finish => {
  const pms = [];

  for (let i = 0; i < vgmstream.subSongCount; ++i) {
    const subSong = vgmstream.selectSubSong(i);
    pms.push(subSong.render());
  }

  Promise.all(pms).then(buffers =>{
    console.log(buffers.map(buf => buf.length))
    finish()
  })
})
