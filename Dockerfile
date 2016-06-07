FROM resources.redborder.lan:5000/rb-cbuilds-n2kafka:1.1
MAINTAINER Diego Fernández

ADD . /app/
RUN source /opt/rh/devtoolset-2/enable; cd /app; /app/configure; make;

RUN yum install -y supervisor && \
    yum install -y python-pip && \
    yum clean all

RUN pip install j2cli
ADD supervisor/n2kafka.conf /etc/supervisor/conf.d/
ADD templates/config.template /app/
ADD bootstrap.sh /app/
WORKDIR /app

CMD ["/app/bootstrap.sh"]


