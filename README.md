# ZenRen


High performance DirectX11 viewer/renderer for Gothic assets (VDF or single files).
<br>Attempts to closely recreate original G1/G2-AddOn look and lighting.

### Download (Windows)
- https://github.com/Katharsas/ZenRen/releases/latest

### Currently supports
- Worldmesh (ZEN)
  - Static Light
    - Indoor: Lightmaps
    - Outdoor: Per-Vertex Baked Brightness 
- Static Objects (VOBs, MOBs)
  - Static Light
    - Indoor: Static Voblight Accumulation (Color & Direction, Visibility Ray Testing)
    - Outdoor: Per-Vob Color from Ground Face
- Sky
  - Time-Of-Day Skylight
  - Sky Layer Animation
- Asset File Formats
  - Level: VDF
  - Objects: VDF
  - Textures: VDF, TGA, PNG

### Features
- Frame Limiter
- Forward Renderer
  - Linear Colors
  - MSAA
  - Transparency Multisampling (Alpha-To-Coverage with Sharpening)
  - Resolution Scaling (up to 4x SSAA equivalent)
- Postprocessing
  - Tonemapping
  - Gamma, Brightness, Contrast
- UI
  - High DPI Aware

### TODO
- Effects
- Dynamic Lights
- Dynamic Objects
  - Light Updates
- Transparent Surfaces (Water)
- Distance Fog
- Sky Celestials (Sun, Moon)
- Asset File Formats
  - Level: ZEN (outside VDF), OBJ
  - Objects: 3DS, OBJ
- Loading single objects without level

### Non-Goals
- Animations
  - Morph-Meshes
  - Skeletal Animations

### Options
<pre>
  --level     Level ZEN name.
                Example: "WORLD.ZEN"
				 
  --vdfDir    Root dir containing VDFs to be loaded (will check all subfolders).
                Example: "C:\Program Files (x86)\Steam\steamapps\common\Gothic"
				 
  --assetDir  Root dir containing additional assets (will check all subfolders).
                Currently supports .TGA files.
                Example: ".\my-mod-assets"
</pre>

### Screenshots
![Oldcamp](Screenshots/oldcamp.jpg)
![Swampcamp](Screenshots/swampcamp.jpg)
![YBerions Temple](Screenshots/yberion_temple.jpg)

## License

**Copyright (c) 2024 - Jan Mothes**

This project's first-party code is "source available" for now.

### Third-Party
- lib/DirectXMath - MIT
- lib/DirectXTex - MIT
- lib/g3log - Public Domain / Unlicense
- lib/imgui - MIT
- lib/magic_enum - MIT
- lib/octree_attcs - MIT
- lib/tinyobj - MIT
- lib/ZenKit - MIT