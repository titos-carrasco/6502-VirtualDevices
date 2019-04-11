import time
import serial

def main():
    # RAM/ROM image from https://github.com/Klaus2m5/6502_65C02_functional_tests
    print( '# Loading RAM/ROM image', flush=True )
    f = open( '/home/roberto/Downloads/6502_functional_test.bin', 'rb' )
    memory = bytearray( f.read( 64*1024 ) )
    f.close()

    # fix RST vector (start address is $0400 - success when addr = 0x3469)
    memory[ 0xFFFC ] = 0x00
    memory[ 0xFFFD ] = 0x04
    last = 0x3469 + 2

    # connecting to the bridge
    print( '# Connecting to the bridge', flush=True )
    ser = serial.Serial( port='/dev/ttyUSB0', baudrate=1000000, timeout=None, write_timeout=None )

    # Bridge Reset
    print( '# Force bridge reset', flush=True )
    ser.setDTR( False )
    time.sleep( 2 )

    # clean up
    ser.reset_output_buffer()
    ser.reset_input_buffer()

    # bridge sync
    print( '# SYNC: sending 0xAA', flush=True )
    ser.write( b'\xAA' );
    ser.flush()
    print( '# SYNC: sending 0xAA', flush=True )
    ser.write( b'\xAA' );
    ser.flush()
    print( '# SYNC: waiting for 0x55', flush=True )
    c = ser.read( 1 )
    if( c != b'\x55' ):
        ser.close()
        return
    print( '# SYNC: OK', flush=True )

    # show time ( nb = USB buffer size? 2ms latency )
    nb = 32
    data_out = bytearray( 1 )
    t0 = time.time()
    cnt = 0
    while( True ):
        data_in = ser.read( nb )
        cmd   = data_in[0]
        addrH = data_in[1]
        addrL = data_in[2]
        addr  = ( ( addrH & 0x00FF) << 8 ) | ( addrL & 0x00FF )
        b     = data_in[3]

        # 'R': read memory from memory
        if( cmd == 0x52 ):
            b = memory[ addr ]
        # 'W': write byte to memory
        elif( cmd == 0x57 ):
            memory[ addr ] = b

        # returns the byte
        data_out[ 0 ] = b
        ser.write( data_out )
        ser.flush()

        # some debug
        cnt = cnt + 1
        print( "%010d %c %04X %02X %f(s)" % ( cnt, cmd, addr, b, time.time() - t0  ), flush=True )
        #print( "%c %04X %02X" % ( cmd, addr, b  ), flush=True )

        if( addr == last ):
            return


# show time
main()
