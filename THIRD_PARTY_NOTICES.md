# Third-party notices

This product includes the following third-party software. Each is fetched at build time
(see the root `CMakeLists.txt`) and is used under its own licence, reproduced or linked below.

## NeuralAmpModelerCore v0.6.0 (Neural Amp Modeler inference)

https://github.com/sdatkinson/NeuralAmpModelerCore — used unmodified to load and run `.nam` captures.

```
MIT License

Copyright (c) 2023 Steven Atkinson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

NeuralAmpModelerCore bundles **nlohmann/json** (MIT, SPDX-FileCopyrightText: 2013 - 2025 Niels Lohmann
<https://nlohmann.me>), which this product also uses to read `.nam` files and the NAM library index.
The licence notice is in the header of `Dependencies/nlohmann/json.hpp` in the NeuralAmpModelerCore source.

## Eigen 3.4.0

https://eigen.tuxfamily.org — Mozilla Public License 2.0 (https://www.mozilla.org/MPL/2.0/).
Compiled with `EIGEN_MPL2_ONLY`, so none of Eigen's LGPL-licensed parts are used.
Eigen is used unmodified; its source is available at the link above.

## JUCE 8

https://juce.com — AGPLv3 or the JUCE commercial licence. See `docs/ARCHITECTURE.md` (licensing)
for what this means for releasing the product.

## NAM captures

No third-party `.nam` captures are shipped with or redistributed by this product. Users import
their own captures, which stay under their creators' terms. The automated tests use only the
example models that ship inside the NeuralAmpModelerCore source (MIT).
