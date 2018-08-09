namespace egg::ovum::utf8 {
  size_t sizeFromLeadByte(uint8_t lead) {
    // Fetch the size in bytes of a codepoint given just the lead byte
    if (lead < 0x80) {
      return 1;
    } else if (lead < 0xC0) {
      return SIZE_MAX; // Continuation byte
    } else if (lead < 0xE0) {
      return 2;
    } else if (lead < 0xF0) {
      return 3;
    } else if (lead < 0xF8) {
      return 4;
    }
    return SIZE_MAX;
  }
  size_t measure(const uint8_t* p, const uint8_t* q) {
    // Return SIZE_MAX if this is not valid UTF-8, otherwise the codepoint count
    size_t count = 0;
    while (p < q) {
      if (*p < 0x80) {
        // Fast code path for ASCII
        p++;
      } else {
        auto remaining = size_t(q - p);
        auto length = sizeFromLeadByte(*p);
        if (length > remaining) {
          // Truncated
          return SIZE_MAX;
        }
        assert(length > 1);
        for (size_t i = 1; i < length; ++i) {
          if ((p[i] & 0xC0) != 0x80) {
            // Bad continuation byte
            return SIZE_MAX;
          }
        }
        p += length;
      }
      count++;
    }
    return count;
  }
}
