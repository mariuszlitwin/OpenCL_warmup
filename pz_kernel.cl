kernel void subrange_hits(__global char *byte_stream,                           // stream of bytes per device
                          __global unsigned int *sub_hits,                      // place for results

                          const unsigned int word_length,                       // word length (in bytes)
                          const unsigned int subrange_length,                   // subrange length (in bytes)

                          const unsigned int val_top,                           // top boundary for value
                          const unsigned int val_bot) {                         // bottom boundary for value
	const unsigned int id = get_global_id(0);
	      unsigned int stop_offset = (id + 1) * subrange_length;
          unsigned int valid_values_count = 0;
          unsigned int value = 0;
    unsigned int i = 0, j = 0;
    
    for (i = id * subrange_length; 
         i < stop_offset;
         i += word_length) {
        value = 0;
        for (j = 0; j < word_length; j++) {
            value <<= 8;
            value |= byte_stream[i+j];
        }
        
        if (value > val_bot && value < val_top)
            valid_values_count++;
    }
    
    sub_hits[id] = valid_values_count;
}
