/** @type {import('node-vgmstream')} */
const vgmstream = require('./lib')
const fs = require('fs')
const path = require('path')

console.log(vgmstream.version);


const buffer = fs.readFileSync(path.join(__dirname, 'test.bank'))

console.log('buffer size: ', buffer.length)

console.log(vgmstream.getSubSongCount(buffer));

const meta = vgmstream.getMeta(buffer, 1)

console.log(meta)
