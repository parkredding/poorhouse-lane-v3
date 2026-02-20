// Mock Web Audio API for testing
const { AudioContext } = require('web-audio-test-api');

// Set up global mocks
global.AudioContext = AudioContext;
global.webkitAudioContext = AudioContext;

// Mock OfflineAudioContext for reverb impulse response generation
global.OfflineAudioContext = class OfflineAudioContext extends AudioContext {
  constructor(numberOfChannels, length, sampleRate) {
    super();
    this.length = length;
    this._numberOfChannels = numberOfChannels;
    this._sampleRate = sampleRate;
  }

  startRendering() {
    return Promise.resolve({
      getChannelData: (channel) => new Float32Array(this.length),
      numberOfChannels: this._numberOfChannels,
      length: this.length,
      sampleRate: this._sampleRate
    });
  }
};

// Suppress console.log in tests unless needed
global.console = {
  ...console,
  log: jest.fn(),
};
