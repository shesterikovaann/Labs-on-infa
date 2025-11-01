#!/bin/bash
PORT=22122
ncat -l $PORT -c 'echo "Сообщение получено" '
