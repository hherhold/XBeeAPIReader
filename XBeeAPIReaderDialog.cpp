#include <QtCore/QDebug>
#include <QString>

#include "XBeeAPIReaderDialog.h"
#include "ui_XBeeAPIReaderDialog.h"

#include <stdio.h>

XBeeAPIReaderDialog::XBeeAPIReaderDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Dialog)
{
    ui->setupUi(this);

    m_serialPortInfoList = QSerialPortInfo::availablePorts();

    foreach (const QSerialPortInfo &info, m_serialPortInfoList)
    {
        ui->serialPortComboBox->addItem(info.portName());
        qDebug() << "Info = " << info.systemLocation();
    }
    ui->connectButton->setEnabled( true );
    ui->disconnectButton->setEnabled( false );
}

XBeeAPIReaderDialog::~XBeeAPIReaderDialog()
{
    delete ui;
}

void XBeeAPIReaderDialog::on_connectButton_clicked()
{
    m_serialPort.setPort( m_serialPortInfoList[ ui->serialPortComboBox->currentIndex() ] );
    m_serialPort.setBaudRate( 9600 );
    m_serialPort.setDataBits( QSerialPort::Data8 );
    m_serialPort.setParity( QSerialPort::NoParity );
    m_serialPort.setStopBits( QSerialPort::OneStop );
    if ( m_serialPort.open( QIODevice::ReadWrite) )
    {
        connect(&m_serialPort, SIGNAL(readyRead()),
                this, SLOT(readResponse()));
        ui->connectButton->setEnabled( false );
        ui->disconnectButton->setEnabled( true );
    }
}

// Called when data is waiting at the serial port to be read.
void XBeeAPIReaderDialog::readResponse()
{
    QByteArray data = m_serialPort.readAll();
    unescapeSerialData( data );

    dumpByteArray( data );

    // Are we in the middle of building a frame?
    if ( m_frameInProgress.length() != 0 )
    {
        // How much more data do we need to complete the frame?
        if ( m_frameInProgress.length() > 3 )
        {
            int totalFrameSize = m_frameInProgress.length() + 4;
            int bytesMissing = totalFrameSize - m_frameInProgress.length();
            if ( data.length() < bytesMissing )
            {
                // Don't have enough to finish the frame, so append what's
                // been received and move on.
                m_frameInProgress.append( data );
                return;
            }
            else if ( data.length() == bytesMissing )
            {
                // Just enough to finish the frame. Add it to the list of
                // completed frames and zero out the frame in progress so
                // we start on a new on next time through.
                m_frameInProgress.append( data );

                m_ZBFrameQueue.push_front( QByteArray( m_frameInProgress ));
                m_frameInProgress.clear();
            }
            else
            {
                // We have enough to finish the frame, and proably a bit more.
                // First finish the frame in progress.
                m_frameInProgress.append( data.left( bytesMissing ));
                data = data.mid( bytesMissing );

                m_ZBFrameQueue.push_front( QByteArray( m_frameInProgress ));
                m_frameInProgress.clear();
            }
        }
    } // if ( m_frameInProgress.length() != 0 )

    // We don't have a frame in progress and we've got some data.
    while ( !data.isEmpty( ))
    {
        int startByteIndex = data.indexOf( 0x7E );
        if ( startByteIndex == -1 )
        {
            // No start byte found, move on.
            data.clear();
            return;
        }

        // To make the math easier, pitch everything in front of the start byte.
        data = data.mid( startByteIndex );

        // At this point we have at least a start byte and no data in progress.
        // Can we get a length?
        int newFrameLength;
        if ( data.length() > 3 )
        {
            newFrameLength = ( data.at( 1 ) << 8 ) + data.at( 2 );
        }
        else
        {
            // Not enough data to get a length. Append what we have and return.
            m_frameInProgress.append( data );
            return;
        }

        // Can we at least finish one frame?
        if ( data.length() > ( newFrameLength + 4 ))
        {
            QByteArray completeFrame;
            completeFrame = data.left( newFrameLength + 4 );

            m_ZBFrameQueue.push_front( completeFrame );

            // Chop off what we've used.
            data = data.mid( newFrameLength + 4 );
        }
        else
        {
            // Can't finish it, so append what we have;
            m_frameInProgress.append( data );
            data.clear();
        }
    }


    qDebug() << "Number of complete frames in the queue = " << m_ZBFrameQueue.size();

}

void XBeeAPIReaderDialog::on_serialPortComboBox_currentIndexChanged(int index)
{
    // Set serial port info to picked one.
    qDebug() << "Combo box index: " << index;
}

void XBeeAPIReaderDialog::on_disconnectButton_clicked()
{
    m_serialPort.close();
    ui->connectButton->setEnabled( true );
    ui->disconnectButton->setEnabled( false );
}


void XBeeAPIReaderDialog::unescapeSerialData( QByteArray& data )
{
    int i = 0;
    while ( ( i = data.indexOf( 0x7D ) ) != -1 )
    {
        QByteArray unescapedBytes;
        unescapedBytes.resize(1);
        unescapedBytes[ 0 ] = data[ i + 1 ] ^ 0x20;
        data.replace( i, 2, unescapedBytes );
    }
}

// Some raw data:
//           1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20
// 7e 00 14 92 00 13 a2 00 40 89 16 20 4b 8e 01 01 00 02 01 00 00 00 00 db
//  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
//
// 7e - start of frame
// 00 14 - length = 20 bytes. Does not include SOF, length, or checksum.
//

void XBeeAPIReaderDialog::dumpByteArray( QByteArray& data )
{
    char buf[ BUFSIZ ];
    QString str;
    for ( int i = 0; i < data.length(); i++ )
    {
        ::sprintf( buf, "%02X ", ( data.at(i) & 0xFF ) );
        str.append( buf );
    }
    qDebug() << str;
}

void XBeeAPIReaderDialog::handleXBeeSerialData( QByteArray& data )
{
    qDebug() << "handleXBeeSerialData called with " << data.length() << " bytes.";
    unescapeSerialData( data );

    qDebug() << "Data:";
    dumpByteArray( data );

    // Start copying in at the start of a frame. This could possibly contain
    // more than one frame of data on startup, or depending on the sample rate or
    // other factors that could delay calling of this function.
    if ( m_frameInProgress.length() == 0 )
    {
        int startByte = data.indexOf( 0x7E );
        m_frameInProgress.append( data.mid( startByte ) );
        qDebug() << "Start byte at offset " << startByte << ". Frame in progress length = " << m_frameInProgress.length();
    }
    else
    {
        // Copy all the data into the frame in progress.
        // << TODO >> This can possibly copy the rest of the current frame and some of the next one.
        // This can result in dropped samples. (I'll worry about this later.)
        qDebug() << "Appending data.";
        m_frameInProgress.append( data );
        qDebug() << data.length() << "bytes appended, m_frameInProgress has " << m_frameInProgress.length() << "bytes.";
    }

    int receivedLength = m_frameInProgress.length();
    int payloadLength = 0;
    // See if the frame in progress is complete. Do we have enough bytes to determine
    // the completed length of this frame? This means we should have the delimiter 0x7E
    // and the first two bytes, making up the length, for a total of 3 bytes.
    if ( receivedLength > 3 )
    {
        // The frame length as counted IN the frame does not include the
        // start byte, the length field (2 bytes), and the checksum byte,
        // for a total of 4 extra bytes of "raw" data.
        payloadLength = ( m_frameInProgress[ 2 ] | ( m_frameInProgress[ 1 ] << 8 ) );
        qDebug() << "receivedLength = " << receivedLength << ". Frame's payload Length = " << payloadLength;

        // Check to see if we need to wait for more data to make up a whole frame.
        if ( receivedLength < ( payloadLength + 4 ) )
        {
            // Wait for more data.
            qDebug() << "Incomplete (so far). Waiting for more data...";
            return;
        }
    }
    else
    {
        // Current frame doesn't have enough bytes to even determine length.
        // Wait for more data.
        return;
    }

    // At this point we should have a completed frame. Verify the checksum.
    int checksumIndex = payloadLength + 4 - 1;
    unsigned char receivedChecksum = m_frameInProgress[ checksumIndex ];
    unsigned int checksum = 0;
    for ( int i = 3; i <= checksumIndex; i++ )
    {
        checksum += m_frameInProgress[ i ];
    }

    if ( ( checksum & 0xFF ) != 0xFF )
    {
        qDebug() << "Checksum mismatch!";

        // Start over by clearing out the frame in progress,
        // then bail out.
        m_frameInProgress.clear();
        return;
    }

    qDebug() << "Checksum OK";

    unsigned char frameType = m_frameInProgress[ 3 ];

    if ( frameType == XBEE_API_FRAME_ZB_IO_DATA_SAMP_RX_IND )
    {
        qDebug() << "Decoding XBEE_API_FRAME_ZB_IO_DATA_SAMP_RX_IND frame";
        QString tmpStr;
        unsigned char msb, lsb;
        ///
        ///
        ///  THIS IS ONLY CORRECT FOR A SINGLE ANALOG SAMPLE. IT DOES NOT
        ///  CHECK SAMPLE MASK OR ANYTHING ELSE TO CHECK FOR OTHER
        ///  POSSIBLE CASES.
        ///
        ///
        msb = m_frameInProgress[ 20 ];
        lsb = m_frameInProgress[ 21 ];
        tmpStr.sprintf( "%x%x", msb, lsb );
        int value = ( msb << 8 ) | lsb;
        qDebug() << "Data frame! Value = " << value << " Hex = " << tmpStr;
        tmpStr.sprintf( "%d", value );
        ui->sensorValueLabel->setText( tmpStr );

    }

    // All done - clear out the frame. If more than one frame's worth of data has been
    // received, this results in dropping samples.
    m_frameInProgress.clear();
}

// 7e 0014 92 0013a200 40891620 4b8e 01 01 00 02 01 00 00 00 00 db

void XBeeAPIReaderDialog::on_debugDataButton_clicked()
{
    const char debugData0[] = { 0x7e, 0x00, 0x14, 0x92, 0x00, 0x13, 0xa2, 0x00,
                               0x40, 0x89, 0x16, 0x20, 0x4b, 0x8e, 0x01, 0x01,
                               0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0xdb };

    const char debugData1[] = { 0x99, 0x7e, 0x00, 0x14, 0x92, 0x00, 0x13, 0xa2, 0x00,
                               0x40, 0x89, 0x16, 0x20, 0x4b, 0x8e, 0x01, 0x01,
                               0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0xdb };


//    QByteArray debugDataArray0( debugData0, 24 );
//    handleXBeeSerialData( debugDataArray0 );

    QByteArray debugDataArray1( debugData1, 25 );
    handleXBeeSerialData( debugDataArray1 );
}



#if 0
        //
        int startByte = data.indexOf( 0x7E );
        if ( startByte != -1 )
        {
            m_frameInProgress.append( data.mid( startByte ) );
            qDebug() << "Start 0x7E byte at offset " << startByte;
        }
        else
        {
            // We're not in the middle of building a frame and this chunk of data
            // doesn't contain a start byte. Ditch this data and come back again
            // with new data that might have a start byte.
            qDebug() << "0x7E not found, moving on.";
            return;
        }
    }
    else
    {
        // There is an incomplete frame in progress.

    }

#endif


#if 0



        // Copy all the data into the frame in progress.
        // << TODO >> This can possibly copy the rest of the current frame and some of the next one.
        // This can result in dropped samples. (I'll worry about this later.)
        qDebug() << "Appending data.";
        m_frameInProgress.append( data );
        qDebug() << data.length() << "bytes appended, m_frameInProgress has " << m_frameInProgress.length() << "bytes.";
    }



    int receivedLength = m_frameInProgress.length();
    int payloadLength = 0;
    // See if the frame in progress is complete. Do we have enough bytes to determine
    // the completed length of this frame? This means we should have the delimiter 0x7E
    // and the first two bytes, making up the length, for a total of 3 bytes.
    if ( receivedLength > 3 )
    {
        // The frame length as counted IN the frame does not include the
        // start byte, the length field (2 bytes), and the checksum byte,
        // for a total of 4 extra bytes of "raw" data.
        payloadLength = ( m_frameInProgress[ 2 ] | ( m_frameInProgress[ 1 ] << 8 ) );
        qDebug() << "receivedLength = " << receivedLength << ". Frame's payload Length = " << payloadLength;

        // Check to see if we need to wait for more data to make up a whole frame.
        if ( receivedLength < ( payloadLength + 4 ) )
        {
            // Wait for more data.
            qDebug() << "Incomplete (so far). Waiting for more data...";
            return;
        }
    }
    else
    {
        // Current frame doesn't have enough bytes to even determine length.
        // Wait for more data.
        return;
    }

    // At this point we should have a completed frame. Verify the checksum.
    int checksumIndex = payloadLength + 4 - 1;
    unsigned char receivedChecksum = m_frameInProgress[ checksumIndex ];
    unsigned int checksum = 0;
    for ( int i = 3; i <= checksumIndex; i++ )
    {
        checksum += m_frameInProgress[ i ];
    }

    if ( ( checksum & 0xFF ) != 0xFF )
    {
        qDebug() << "Checksum mismatch!";

        // Start over by clearing out the frame in progress,
        // then bail out.
        m_frameInProgress.clear();
        return;
    }

    qDebug() << "Checksum OK";










    // Search for the start of a frame.

    QByteArray apiFrame;
    // Make an API frame. First, search for the start delimiter.
    char c = 0x0;
    int bytesRead = 0;
    do {
        bytesRead = m_serialPort.read( &c, 1 );
        if ( bytesRead == -1 )
        {
            qDebug() << "Read failure!";
        }
        else
        {
            qDebug() << "Bytes read = " << bytesRead;
        }
    } while (c != 0x7e);

    apiFrame.append( c );

    // Next two bytes are the length.
    unsigned int frameDataLength = 0;
    m_serialPort.read( &c, 1 );
    apiFrame.append( c );
    frameDataLength = c << 8;
    m_serialPort.read( &c, 1 );
    apiFrame.append( c );
    frameDataLength += c;

    char buf[ BUFSIZ ];
    m_serialPort.read( buf, frameDataLength );

    apiFrame.append( buf, frameDataLength );

    // Last byte is checksum.
    m_serialPort.read( &c, 1 );
    apiFrame.append( c );

    dumpByteArray( apiFrame );
#endif
#if 0
    QByteArray data = m_serialPort.readAll();
    QString tmpStr;
    handleXBeeSerialData( data );
#endif
