#ifndef XBEEAPIREADERDIALOG_H
#define XBEEAPIREADERDIALOG_H

#include <QQueue>
#include <QDialog>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

#define XBEE_API_FRAME_AT_COMMAND                   0x08  /* AT Command */
#define XBEE_API_FRAME_AT_COMMAND_QUEUE_PARAM_VALUE 0x09  /* AT Command - Queue Parameter Value */
#define XBEE_API_FRAME_ZB_XMT_RQST                  0x10  /* ZigBee Transmit Request */
#define XBEE_API_FRAME_XPLICIT_ADDR_ZB_CMD_FRAME    0x11  /* Explicit Addressing ZigBee Command Frame */
#define XBEE_API_FRAME_REMOTE_CMD_RQST              0x17  /* Remote Command Request*/
#define XBEE_API_FRAME_CREATE_SRC_ROUTE             0x21  /* Create Source Route */
#define XBEE_API_FRAME_AT_CMD_RESP                  0x88  /* AT Command Response */
#define XBEE_API_FRAME_MODEM_STATUS                 0x8A  /* Modem Status */
#define XBEE_API_FRAME_ZB_XMT_STATUS                0x8B  /* ZigBee Transmit Status */
#define XBEE_API_FRAME_ZB_RECV_PKT_AO_0             0x90  /* ZigBee Receive Packet (AO=0) */
#define XBEE_API_FRAME_ZB_XPLICIT_RX_IND_AO_1       0x91  /* ZigBee Explicit Rx Indicator (AO=1) */
#define XBEE_API_FRAME_ZB_IO_DATA_SAMP_RX_IND       0x92  /* ZigBee IO Data Sample Rx Indicator */
#define XBEE_API_FRAME_XB_SENSOR_RD_IND             0x94  /* XBee Sensor Read Indicator (AO=0) */
#define XBEE_API_FRAME_NODE_ID_IND_AO_0             0x95  /* Node Identification Indicator (AO=0) */
#define XBEE_API_FRAME_REMOTE_CMD_RESP              0x97  /* Remote Command Response */
#define XBEE_API_FRAME_OVER_AIR_FW_UPDATE_STATUS    0xA0  /* Over-the-Air Firmware Update Status */
#define XBEE_API_FRAME_ROUTE_RECORD_IND             0xA1  /* Route Record Indicator */

namespace Ui {
class Dialog;
}

class XBeeAPIReaderDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit XBeeAPIReaderDialog(QWidget *parent = 0);
    ~XBeeAPIReaderDialog();

private slots:
    void on_connectButton_clicked();

    void on_serialPortComboBox_currentIndexChanged(int index);

    void readResponse( );

    void on_disconnectButton_clicked();

    void on_debugDataButton_clicked();

private:
    Ui::Dialog *ui;
    QSerialPort m_serialPort;
    QList<QSerialPortInfo> m_serialPortInfoList;

    void handleXBeeSerialData( QByteArray& data );

    void unescapeSerialData( QByteArray& data );
    void dumpByteArray( QByteArray& data );

    QQueue< QByteArray > m_ZBFrameQueue;

    void decodeZBAPIFrame( QByteArray& completeFrame );

    QByteArray m_frameInProgress;
};

#endif // XBEEAPIREADERDIALOG_H


