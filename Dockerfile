ARG MANYLINUX=2010
FROM quay.io/pypa/manylinux${MANYLINUX}_x86_64

ARG PYTHON_VERSION=36
ENV PYBIN /opt/python/cp${PYTHON_VERSION}-cp${PYTHON_VERSION}*/bin

RUN curl -L https://github.com/Kitware/CMake/releases/download/v3.16.2/cmake-3.16.2-Linux-x86_64.tar.gz | tar xz -C /usr/ --strip-components=1 && \
${PYBIN}/pip3 install -U pip auditwheel dqcsim 

WORKDIR /io
ENV DQCSIM_LIB /opt/python/cp36-cp36m/lib/libdqcsim.so
ENV DQCSIM_INC /opt/python/cp36-cp36m/include
ENTRYPOINT ["bash", "-c", "${PYBIN}/python3 setup.py bdist_wheel"]
