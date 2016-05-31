FROM resources.redborder.lan:5000/rb-cbuilds-n2kafka:1.1
MAINTAINER Diego Fernández

ADD . /app/
RUN source /opt/rh/devtoolset-2/enable; cd /app; /app/configure; make;

RUN apt-get install -y supervisor && \
    apt-get install -y python-pip && \
    rm -rf /var/lib/apt/lists/* && \
    apt-get clean

RUN pip install j2cli
ADD supervisor/n2kafka.conf /etc/supervisor/conf.d/
ADD templates/config.template /app/
ADD bootstrap.sh /app/
WORKDIR /app

CMD ["/app/bootstrap.sh"]


