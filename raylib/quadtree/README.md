# raylib: simple 2d quadtree

## Building for desktop

from *flux workspace dir*:
```cmd
./flux/flux build -config=release -target=desktop flux-samples/raylib/quadtree
```

## Building for web

from *flux workspace dir*:
```cmd
./flux/flux build -config=release -target=emscripten flux-samples/raylib/quadtree
```

launch server from *output dir*:
```cmd
python wasm-server.py
```
 open http://localhost:8080 to web browser

## Usage

Use directional keys ⬅⬆⬇➡ and mouse 🖱 to move and oriente camera.
