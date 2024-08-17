/** @type {import('node-vgmstream')} */
const { VGMStream } = require('./lib')
const fs = require('fs')
const path = require('path')

console.log(VGMStream.version);

const buffer = fs.readFileSync(path.join(__dirname, 'test.bank'))

console.log('buffer size: ', buffer.length)

const vgmstream = new VGMStream(buffer);

console.log(vgmstream.getSubSongCount());

const meta = vgmstream.getMeta(1)
console.log(meta)

fs.writeFileSync('a.wav', vgmstream.wave(1))

