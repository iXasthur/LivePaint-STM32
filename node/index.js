const { com, rate, width, height } = require('./config.json');

const SerialPort = require('serialport');
const port = new SerialPort(com, { baudRate: rate });
const decoder = new TextDecoder("utf-8");

const fs = require('fs');
const { createCanvas } = require('canvas');
const canvas = createCanvas(width, height);
const ctx = canvas.getContext('2d');

port.on('error', function (err) {
  console.log('Error: ', err.message);
});

let str = '';

port.on('data', function (data) {
    const newChar = decoder.decode(data);
    str += newChar;
    if (newChar === '\0') {
      if (str.indexOf('CLEAR') > -1) {
        ctx.clearRect(0, 0, width, height);
        fs.writeFile('img.png', canvas.toBuffer(), () => {
          console.log(`[${new Date().toString().replace(/ GMT.*/g, '')}]`, 'Processed: CLEAR');
        });
        str = '';
        return;
      }

      const [x, y, radius, color] = str.match(/[0-9]+/g).map(e => Number(e));

      ctx.fillStyle = `#${color.toString(16).slice(2, 8)}${color.toString(16).slice(0, 2)}`;
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, 2 * Math.PI);
      ctx.fill();
      
      fs.writeFile('img.png', canvas.toBuffer(), () => {
        console.log(`[${new Date().toString().replace(/ GMT.*/g, '')}]`, 
          `Processed: x = ${x} y = ${y} radius = ${radius} color = #${color.toString(16).slice(2, 8)}${color.toString(16).slice(0, 2)}`);
      });
      str = '';
    }
});

console.log('Write data to image is running\n');
