/** @type {import('node-vgmstream')} */
const lib = require('./lib')

const { VGMStream } = lib

const fs = require('fs')
const path = require('path')

console.log(VGMStream.version);

const buffer = fs.readFileSync(path.join(__dirname, 'test.bank'))

console.log('source buffer size: ', buffer.length)

const vgmstream = new VGMStream(buffer);

console.log('total sub songs: ', vgmstream.subSongCount);

const subSong = vgmstream.selectSubSong(1)

console.log(subSong.info)

const wave = subSong.renderToWave()

console.log('rendered buffer size: ', wave.length)

fs.writeFileSync('a.wav', wave)

