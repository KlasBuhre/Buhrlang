import "Trace"

class SfwEventDescription(int dialogId)

interface ISfwEventDispatcherObserver {
     SfwEventDescription getEvent()
}

class ISfwEventDispatcher {
    var Option<ISfwEventDispatcherObserver> mObserver = None

    requestEventTimeslot(ISfwEventDispatcherObserver observer) {
        mObserver = Some(observer)
    }
}

class ISessionContext(ISfwEventDispatcher mEventDispatcher) {
    ISfwEventDispatcher getISfwEventDispatcher() {
        return mEventDispatcher
    }

    int getIncomingCallDialogId() {
        return 1
    }
}

enum Primitive {
    Continue,
    Stop
}

class ServicePrimitive(Primitive mPrimitive)

class ISip

class IEvent(ISessionContext mSessionContext) {
    ISip getISip() {
        return new ISip
    }

    ISessionContext getSessionContext() {
        return mSessionContext
    }
}

class IInviteEvent(arg ISessionContext sessionContext): IEvent(sessionContext)
class INotifyEvent(arg ISessionContext sessionContext): IEvent(sessionContext)
class INotifyAcceptEvent(arg ISessionContext sessionContext): IEvent(sessionContext)

class ServicesUtils {
    static string getIcidValue(IEvent event) {
        return "1234"
    }
}

class SipUtilities {
    static string EventType_EmergencyCall = "emergencyCall"
    static string EventParam_Start = "start"
    static string EventParam_Stop = "stop"

    static string getEventHeaderType(ISip iSip) {
        return EventType_EmergencyCall
    }

    static string[] getEventHeaderParams(ISip iSip) {        
        return [ EventParam_Start, EventParam_Stop ]
    }
}

class OamMoData(var bool mLocked)

class EmergencyStateHandler {
    static EmergencyStateHandler getInstance() {
        return new EmergencyStateHandler
    }

    emergencyCallStart(string icid) { 
        println("EmergencyStateHandler: start, icid = " + icid) 
    }

    emergencyCallStop(string icid) { 
        println("EmergencyStateHandler: stop, icid = " + icid) 
    }
}

