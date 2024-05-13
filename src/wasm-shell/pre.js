if (typeof SharedArrayBuffer === 'undefined') {
  const dummyMemory = new WebAssembly.Memory({initial : 0, maximum : 0, shared : true})
  globalThis.SharedArrayBuffer = dummyMemory.buffer.constructor
}

Module = {};

window.onload = () => {
  let element = document.getElementById('output');
  Module['print'] = (text) => {
    if (text.length > 1)
      text = Array.prototype.slice.call(text).join('');
    console.log("log: " + text);
    if (element) {
      element.innerText += text + '\n';
      element.scrollTop = element.scrollHeight; // focus on bottom
    }
  };
  Module['printErr'] = Module['print'];

  Module.canvas = (function() {
    var canvas = document.getElementById('canvas');
    return canvas;
  })();
};