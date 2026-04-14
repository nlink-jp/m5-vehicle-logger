// data_buffer.h — Ring buffer for sensor records
#pragma once

#include "types.h"
#include "config.h"

class DataBuffer {
public:
  DataBuffer() : _head(0), _tail(0), _count(0) {}

  bool push(const SensorRecord& record) {
    _buffer[_head] = record;
    _head = (_head + 1) % DATA_BUFFER_SIZE;
    if (_count < DATA_BUFFER_SIZE) {
      _count++;
    } else {
      // Overwrite oldest — advance tail
      _tail = (_tail + 1) % DATA_BUFFER_SIZE;
    }
    return true;
  }

  bool peek(SensorRecord& out) const {
    if (_count == 0) return false;
    out = _buffer[_tail];
    return true;
  }

  bool pop(SensorRecord& out) {
    if (_count == 0) return false;
    out = _buffer[_tail];
    _tail = (_tail + 1) % DATA_BUFFER_SIZE;
    _count--;
    return true;
  }

  // Pop up to maxCount records into the provided array. Returns actual count.
  int popBatch(SensorRecord* out, int maxCount) {
    int popped = 0;
    while (popped < maxCount && _count > 0) {
      out[popped] = _buffer[_tail];
      _tail = (_tail + 1) % DATA_BUFFER_SIZE;
      _count--;
      popped++;
    }
    return popped;
  }

  int count() const { return _count; }
  int capacity() const { return DATA_BUFFER_SIZE; }
  bool isFull() const { return _count >= DATA_BUFFER_SIZE; }
  bool isEmpty() const { return _count == 0; }

private:
  SensorRecord _buffer[DATA_BUFFER_SIZE];
  int _head;
  int _tail;
  int _count;
};
