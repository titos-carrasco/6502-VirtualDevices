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
    success = 0x3469

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

    # show time
    ba = bytearray( 1 )
    t0 = time.time()
    while( True ):
        cmd   = ser.read( 1 )[0]
        addrH = ser.read( 1 )[0]
        addrL = ser.read( 1 )[0]
        addr  = ( ( addrH<<8 ) & 0xFF00 ) | ( addrL & 0xFF )
        b     = ser.read( 1 )[0]

        # 'R': red memory from memory
        if( cmd == 0x52 ):
            b = memory[ addr ]
            ba[ 0 ] = b
            ser.write( ba )
            ser.flush()
        # 'W': write byte to memory
        elif( cmd == 0x57 ):
            memory[ addr ] = b
            ba[ 0 ] = b
            ser.write( ba )
            ser.flush()

        # some debug
        print( "%c %04X %02X" % ( cmd, addr, b ) )

        # test end
        if( addr == success + 2 ):
            print( '# SUCESS: %f (s)' % ( time.time() - t0 ), flush = True )
            while( True ):
                pass

# show time
main()
