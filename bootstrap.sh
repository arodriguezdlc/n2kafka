#!/bin/bash

j2 /app/config.template > /app/config.json
sleep 5
supervisord -n

