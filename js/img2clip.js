const { clipboard, nativeImage } = require('electron');
const fs = require('fs');

fs.watchFile('./img.png', (eventType, filename) => {
    const image = nativeImage.createFromPath('./img.png');
    clipboard.clear();
    clipboard.writeImage(image);
    console.log(`[${new Date().toString().replace(/ GMT.*/g, '')}]`, 'Copied to clipboard');
})

console.log('Copy to clipboard is running\n');
