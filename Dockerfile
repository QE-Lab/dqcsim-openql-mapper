ARG MANYLINUX=2010
FROM quay.io/pypa/manylinux${MANYLINUX}_x86_64

ARG PYTHON_VERSION=36
ENV PYBIN /opt/python/cp${PYTHON_VERSION}-cp${PYTHON_VERSION}*/bin

RUN curl -L https://github.com/Kitware/CMake/releases/download/v3.16.2/cmake-3.16.2-Linux-x86_64.tar.gz | tar xz -C /usr/ --strip-components=1 && \
${PYBIN}/pip3 install -U pip auditwheel==3.0.0 dqcsim && \
echo "214a215" > auditwheel.patch && \
echo ">         remove_platforms = list(remove_platforms)" >> auditwheel.patch && \
patch /opt/_internal/cpython-3.6.10/lib/python3.6/site-packages/auditwheel/wheeltools.py auditwheel.patch

ENV DQCSIM_LIB /opt/python/cp36-cp36m/lib/libdqcsim.so
ENV DQCSIM_INC /opt/python/cp36-cp36m/include
ADD . .
ENTRYPOINT ["bash", "-c", "${PYBIN}/python3 setup.py bdist_wheel && ${PYBIN}/python3 -m auditwheel addtag -w /io/dist target/python/dist/*.whl"]
