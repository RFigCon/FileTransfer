
namespace ServerCodes{
    enum ServerCode{
        TYPE_MASK = 0x0F,
        FILE = 0x00, //0x00
        INFO = 0x01, //0x01
        //INPUT = 0x02, //0x02
        
        CON_MASK = 0xF0,
        CON_ON = 0x00, //0x00
        CON_OFF = 0x10 //0x10
    };
};
