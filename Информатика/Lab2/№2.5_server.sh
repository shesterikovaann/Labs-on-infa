#!/bin/bash
PORT=22122
nc -l $PORT -c 'echo "Сообщение получено" '
