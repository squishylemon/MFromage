# fromage

fromage is a voxel engine implementation for the PlayStation 1 video game console.

## Compilation (2024)

At this time, fromage is **very** unsupported. You're on your own with this one - I can't promise I will be able to help.

Requirements:

* Linux
* [Wonderful Toolchain](https://wonderful.asie.pl/docs/getting-started/)
* Python 3 and requisite dependencies: python3-numpy, python3-scipy, python3-Pillow
* mkisofs

1. Install the MIPS GCC toolchain from Wonderful: `wf-pacman -S toolchain-gcc-mipsel-elf`
2. Clone candyk-psx to be in a directory adjacent to fromage: `cd ..`, `git clone https://github.com/ChenThread/candyk-psx`
3. Compile candyk-psx using the Wonderful toolchain: `cd candyk-psx`, `export PATH=/opt/wonderful/toolchain/gcc-mipsel-elf/bin:$PATH`, `make`
4. Compile Fromage: `cd ../fromage`, `make TYPE=exe REGION=europe`

Or use this docker.
```
FROM debian:12.5 AS build

# Install dependencies
RUN apt update && apt install -y \
    python3-numpy python3-scipy python3-pillow \
    genisoimage git wget make \
    libavutil-dev libavcodec-dev libavformat-dev libswscale-dev \
    build-essential && \
    rm -rf /var/lib/apt/lists/*

# Set up Wonderful Toolkit
WORKDIR /opt/wonderful
ENV WURL="https://wonderful.asie.pl/bootstrap/wf-bootstrap-x86_64.tar.gz"
RUN wget -O- "$WURL" | tar -xzC /opt/wonderful

ENV PATH="/opt/wonderful/bin:${PATH}"
ENV WONDERFUL_TOOLCHAIN="/opt/wonderful"

# Update and install toolchain
RUN wf-pacman -Syy && wf-pacman --noconfirm -Syu
RUN ln -s /proc/mounts /etc/mtab
RUN yes '' | wf-pacman -S toolchain-gcc-mipsel-elf
ENV PATH="/opt/wonderful/toolchain/gcc-mipsel-elf/bin:${PATH}"

# Clone and build CandyK-PSX
WORKDIR /candyk-psx

RUN git clone https://github.com/ChenThread/candyk-psx .
RUN git checkout 5d91befa && make

# Clone and build MFromage
WORKDIR /MFromage

RUN git clone https://github.com/squishylemon/MFromage .
RUN mkdir obj out xml_compile

RUN make clean && make music.hdr && make music.xa && make sounds.lz4 && make TYPE=exe REGION=usa && mv boot.exe out/boot-usa.exe

RUN make clean && make music.hdr && make music.xa && make sounds.lz4 && make TYPE=exe REGION=europe && mv boot.exe out/boot-eur.exe

RUN mv music.xa xml_compile/music.xa && mv music.hdr xml_compile/music.hdr && mv sounds.lz4 xml_compile/sounds.lz4 && mv atlas.lz4 xml_compile/atlas.lz4 && mv usa.xml out/usa.xml && mv eur.xml out/eur.xml && mv system.cnf xml_compile/system.cnf && mv LICENSEE.DAT xml_compile/LICENSEE.DAT
# Final stage - copy built binaries
FROM scratch AS binaries
COPY --from=build /MFromage/out /
COPY --from=build /MFromage/xml_compile /xml_compile
```