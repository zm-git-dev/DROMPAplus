# DROMPAplus

#1. Overview
DROMPA (DRaw and Observe Multiple enrichment Profiles and Annotation) is a program for user-friendly and flexible ChIP-seq pipelining. DROMPA can be used for quality check, PCRbias filtering, normalization, peak calling, visualization and other multiple analyses of ChIP-seq data. DROMPA is specially designed so that it is easy to handle, and for users without a strong bioinformatics background.

#2. Install
DROMPAplus is written in C++ and requires the following programs and libraries:
* [Boost C++ library](http://www.boost.org/)
* [Cairo libraries](http://www.cairographics.org/)
* [GTK library](http://www.gtk.org/)
* [GNU Scientific Library](http://www.gnu.org/software/gsl/)
* [zlib](http://www.zlib.net/)
* [SAMtools](http://samtools.sourceforge.net/) (for BAM formatted input)
* [R](http://www.r-project.org/) (for PROFILE command)

#### 2.1. Install required libraries
for Ubuntu:

    sudo apt-get install git build-essential libgtk2.0-dev libboost-all-dev \
    libgsl-dev libz-dev samtools r-base
 
for CentOS:

    sudo yum -y install git gcc-c++ boost-devel zlib-devel gsl-devel gtk2-devel
and install samtools from [the website](http://samtools.sourceforge.net/).

#### 2.2. Install cpdf
 DROMPA uses Coherent PDF (http://community.coherentpdf.com/) for merging pdf files.
 
     wget http://github.com/coherentgraphics/cpdf-binaries/archive/master.zip
     unzip master.zip
    
#### 2.3. Install DROMPA
    git clone https://github.com/rnakato/DROMPAplus.git
    cd DROMPAplus
    git clone https://github.com/rnakato/SSP.git src/SSP
    make

If you get an installation error, make sure that all required libraries are installed.

#### 2.4. Add the PATH environment variable
For example, if you downloaded DROMPA and cpdf into the $HOME/my_chipseq_exp directory, type:

    export PATH = $PATH:$HOME/my_chipseq_exp/DROMPAplus
    export PATH = $PATH:$HOME/my_chipseq_exp/cpdf-binaries-master/Linux-Intel-**bit

#3. Usage
 See Manual.pdf for detail.

#4. Reference
1. Nakato R., Shirahige K. Recent advances in ChIP-seq analysis: from quality management to whole-genome annotation, Briefings in Bioinformatics, 2016.

2. Nakato, R., Itoh T. and Shirahige K.: DROMPA: easy-to-handle peak calling and visualization software for the computational analysis and validation of ChIP-seq data, Genes to Cells, vol.18, issue 7, 2013.
