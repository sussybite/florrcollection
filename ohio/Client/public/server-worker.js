// Web Worker Server for Singleplayer Mode
// This worker loads and runs the compiled server WASM

var serverModule = null;
var serverReady = false;
var clientConnected = false;
var wsId = 0;
var tickInterval = null;

// Message handler from main thread
self.onmessage = function(e) {
    var type = e.data.type;
    var data = e.data.data;
    
    // console.log('Worker: Received message from main thread, type:', type);
    
    switch(type) {
        case 'connect':
            handleConnect();
            break;
        case 'message':
            if (serverReady && serverModule) {
                handleMessage(new Uint8Array(data));
            } else {
                console.warn('Worker: Message received but server not ready. serverReady:', serverReady, 'serverModule:', !!serverModule);
            }
            break;
        case 'disconnect':
            handleDisconnect();
            break;
    }
};

function handleConnect() {
    if (clientConnected) return;
    
    console.log('Worker: Loading server WASM...');
    
    // Import the server WASM module
    // Emscripten modules in workers need to be loaded differently
    try {
        importScripts('gardn-server.js');
        
        // Emscripten creates Module as a function that needs to be called
        // Wait for it to be available
        function waitForModule() {
            if (typeof Module !== 'undefined') {
                // Module might be a function (newer Emscripten) or an object
                if (typeof Module === 'function') {
                    // It's a function, call it with config
                    initializeServerWithFunction(Module);
                } else {
                    // It's already an object, use it directly
                    initializeServerWithObject(Module);
                }
            } else {
                setTimeout(waitForModule, 10);
            }
        }
        waitForModule();
    } catch (error) {
        console.error('Worker: Error loading server WASM:', error);
        self.postMessage({
            type: 2, // disconnect
            len: 1006,
            reason: "Failed to load server: " + error.message
        });
    }
}

function initializeServerWithFunction(ModuleFunc) {
    try {
        // Configure Module for worker environment
        var moduleConfig = {
            locateFile: function(path) {
                if (path.endsWith('.wasm')) {
                    return 'gardn-server.wasm';
                }
                return path;
            },
            onRuntimeInitialized: function() {
                // Store reference to the module instance
                serverModule = this;
                setupServer();
            }
        };
        
        // Call the Module function to initialize
        ModuleFunc(moduleConfig);
        
    } catch (error) {
        console.error('Worker: Error initializing server:', error);
        self.postMessage({
            type: 2, // disconnect
            len: 1006,
            reason: "Failed to initialize server: " + error.message
        });
    }
}

function initializeServerWithObject(ModuleObj) {
    try {
        // Module is already an object, configure it
        if (ModuleObj.locateFile) {
            var originalLocateFile = ModuleObj.locateFile;
            ModuleObj.locateFile = function(path) {
                if (path.endsWith('.wasm')) {
                    return 'gardn-server.wasm';
                }
                return originalLocateFile ? originalLocateFile(path) : path;
            };
        } else {
            ModuleObj.locateFile = function(path) {
                if (path.endsWith('.wasm')) {
                    return 'gardn-server.wasm';
                }
                return path;
            };
        }
        
        var originalOnRuntimeInitialized = ModuleObj.onRuntimeInitialized;
        ModuleObj.onRuntimeInitialized = function() {
            if (originalOnRuntimeInitialized) originalOnRuntimeInitialized();
            setupServer();
        };
        
        serverModule = ModuleObj;
        
    } catch (error) {
        console.error('Worker: Error initializing server:', error);
        self.postMessage({
            type: 2, // disconnect
            len: 1006,
            reason: "Failed to initialize server: " + error.message
        });
    }
}

function setupServer() {
    try {
        serverReady = true;
        
        // Set up ws_connections object to intercept sends
        serverModule.ws_connections = {};
        
        // Create a proxy WebSocket-like object for the single client
        var wsProxy = {
            send: function(data) {
                // data is a Uint8Array from HEAPU8.subarray
                // Convert to regular array for postMessage
                var dataArray;
                if (data instanceof Uint8Array) {
                    dataArray = Array.from(data);
                } else if (data instanceof ArrayBuffer) {
                    dataArray = Array.from(new Uint8Array(data));
                } else {
                    // Assume it's already an array or array-like
                    dataArray = Array.from(data);
                }
                
                // Send packet back to main thread
                self.postMessage({
                    type: 1, // message type
                    len: dataArray.length,
                    data: dataArray
                });
                
                // console.log('Worker: Sent packet to client, length:', dataArray.length);
            },
            close: function(code, reason) {
                self.postMessage({
                    type: 2, // disconnect type
                    len: code || 1000,
                    reason: reason || ""
                });
            }
        };
        
        // Store the proxy for our single client (ws_id = 0)
        serverModule.ws_connections[wsId] = wsProxy;
        
        // Initialize the server (this creates the game instance)
        serverModule._main();
        
        // Connect the client (ws_id = 0 for single client)
        // This will call client->init() which adds the client to the game
        serverModule._on_connect(wsId);
        
        // Start the tick loop - this sends client updates every tick
        // TPS is 20 ticks per second (from Shared/StaticData.cc)
        var TPS = 20;
        var tickDelay = 1000 / TPS; // 50ms per tick
        var lastTickTime = performance.now();
        var accumulatedTime = 0;
        
        function tickLoop() {
            if (!serverReady || !serverModule) return;
            
            var now = performance.now();
            var deltaTime = now - lastTickTime;
            lastTickTime = now;
            
            // Accumulate time and run ticks as needed
            accumulatedTime += deltaTime;
            
            // Run ticks for accumulated time (catch up if behind)
            var ticksToRun = Math.floor(accumulatedTime / tickDelay);
            accumulatedTime -= ticksToRun * tickDelay;
            
            // Cap at 5 ticks max per frame to prevent spiral of death
            ticksToRun = Math.min(ticksToRun, 5);
            
            for (var i = 0; i < ticksToRun; i++) {
                try {
                    serverModule._tick();
                } catch (error) {
                    console.error('Worker: Error in tick:', error);
                }
            }
            
            // Schedule next frame
            setTimeout(tickLoop, Math.max(0, tickDelay - (performance.now() - now)));
        }
        
        // Start the tick loop
        tickLoop();
        
        console.log('Worker: Tick loop started at', TPS, 'TPS (', tickDelay, 'ms per tick)');
        
        clientConnected = true;
        
        // Send connection event to main thread
        self.postMessage({
            type: 0, // connected
            len: 0,
            data: null
        });
        
        console.log('Worker: Server initialized and ready');
        
    } catch (error) {
        console.error('Worker: Error setting up server:', error);
        self.postMessage({
            type: 2, // disconnect
            len: 1006,
            reason: "Failed to setup server: " + error.message
        });
    }
}

function handleMessage(data) {
    if (!serverReady || !serverModule) {
        console.warn('Worker: Received message but server not ready');
        return;
    }
    
    // Get the address of INCOMING_BUFFER from the server
    var bufferPtr = serverModule._get_incoming_buffer();
    if (!bufferPtr) {
        console.error('Worker: Failed to get incoming buffer address');
        return;
    }
    
    var MAX_BUFFER_LEN = 1024; // From Wasm.cc
    var len = Math.min(data.length, MAX_BUFFER_LEN);
    
    // console.log('Worker: Received message from client, length:', len, 'first byte:', data[0]);
    // console.log('Worker: Buffer pointer:', bufferPtr, 'HEAPU8 available:', !!serverModule.HEAPU8);
    
    // Copy data to the server's INCOMING_BUFFER
    // In Emscripten, HEAPU8 should be available after runtime initialization
    var heapU8 = serverModule.HEAPU8;
    if (!heapU8) {
        // Try alternative access methods
        if (serverModule.wasmMemory) {
            heapU8 = new Uint8Array(serverModule.wasmMemory.buffer);
        } else if (serverModule.HEAP) {
            heapU8 = new Uint8Array(serverModule.HEAP);
        } else {
            console.error('Worker: HEAPU8 not available. Module keys:', Object.keys(serverModule).slice(0, 20));
            console.error('Worker: wasmMemory:', !!serverModule.wasmMemory, 'HEAP:', !!serverModule.HEAP);
            return;
        }
    }
    
    // Copy data directly to the buffer at the specified pointer
    try {
        for (var i = 0; i < len; i++) {
            heapU8[bufferPtr + i] = data[i];
        }
        // console.log('Worker: Copied', len, 'bytes to buffer at', bufferPtr);
    } catch (error) {
        console.error('Worker: Error copying to buffer:', error, 'bufferPtr:', bufferPtr, 'len:', len);
        return;
    }
    
    // Call server's on_message function
    serverModule._on_message(wsId, len);
    
    // Note: Updates will be sent on the next scheduled tick
    // The tick loop runs at 20 TPS, so updates will arrive within 50ms
}

function handleDisconnect() {
    clientConnected = false;
    serverReady = false;
    
    if (tickInterval) {
        clearInterval(tickInterval);
        tickInterval = null;
    }
    
    if (serverModule && serverModule._on_disconnect) {
        serverModule._on_disconnect(wsId, 1000);
    }
    
    wsId = 0;
}

// Initialize
console.log('Server worker initialized');
