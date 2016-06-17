FROM redborder/builds-n2kafka:latest
MAINTAINER Diego Fernández

RUN yum install -y supervisor && \
    yum install -y python-pip && \
    yum clean all

ADD . /app/
RUN cd /app; /app/configure; make;

RUN pip install j2cli
ADD supervisor/n2kafka.ini /etc/supervisord.d/
ADD templates/config.template /app/
ADD bootstrap.sh /app/
WORKDIR /app

CMD ["/app/bootstrap.sh"]


