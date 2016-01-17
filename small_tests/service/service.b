import "sfw"
import "Trace"

enum ServiceState {
    Idle,
    EmergencyCallStartReceived,
    EmergencyCallStopReceived,
    WaitingToSendNotifyReject,
    Guard
}

use ServiceState

class EmergencyStateService(OamMoData mOamMoData): ISfwEventDispatcherObserver {
    var mState = Idle
    var mIcid = ""
    var Option<ISessionContext> mSessionContext = None

    static string getServiceStateString(ServiceState serviceState) {
        return match serviceState {
            Idle                       -> "Idle",
            EmergencyCallStartReceived -> "EmergencyCallStartReceived",
            EmergencyCallStopReceived  -> "EmergencyCallStopReceived",
            WaitingToSendNotifyReject  -> "WaitingToSendNotifyReject",
            Guard                      -> "Guard"
        }
    }
    
    ServicePrimitive handleEvent(IEvent iEvent) {
        println("Emergency State Service received event in state " + 
                getServiceStateString(mState))
        
        return match iEvent {
            INotifyEvent notify             -> handleNotify(notify),
            INotifyAcceptEvent notifyAccept -> handleNotifyAccept(notifyAccept),
            _ -> {
                println("Invalid Event received for this service. Returning CONTINUE")
                new ServicePrimitive(Primitive.Continue)
            }
        }
    }

    ServicePrimitive handleNotify(INotifyEvent notify) {
        if mOamMoData.mLocked {
            println("The Emergency State Service is locked.")
            return rejectNotify(notify)
        }

        println("The Emergency State Service is unlocked.")

        return match mState {
            Idle -> handleNotifyInIdleState(notify),
            _ -> {
                println("Notify received in unexpected state " + 
                        getServiceStateString(mState) + 
                        " for EmergencyStateService.")
                new ServicePrimitive(Primitive.Continue)
            }
        }
    }

    ServicePrimitive handleNotifyAccept(INotifyAcceptEvent notifyAccept) {
        match mState {
            EmergencyCallStartReceived -> {
                EmergencyStateHandler.getInstance.emergencyCallStart(mIcid)
                setServiceState(Guard)
            },
            EmergencyCallStopReceived -> {
                EmergencyStateHandler.getInstance.emergencyCallStop(mIcid)
                setServiceState(Guard)
            },
            _ -> {}
        }
        return new ServicePrimitive(Primitive.Continue)
    }

    ServicePrimitive handleNotifyInIdleState(INotifyEvent notify) {
        mIcid = ServicesUtils.getIcidValue(notify)
        if mIcid.empty() {
            println("Could not get ICID value from event.")
            return new ServicePrimitive(Primitive.Continue)
        }

        if SipUtilities.getEventHeaderType(notify.getISip) ==
           SipUtilities.EventType_EmergencyCall {
            let parameters = SipUtilities.getEventHeaderParams(notify.getISip)
            if parameters.size > 0 {
                match parameters[0] {
                    SipUtilities.EventParam_Start -> {
                        println("Emergency call start.")
                        setServiceState(EmergencyCallStartReceived)
                    },
                    SipUtilities.EventParam_Stop -> {
                        println("Emergency call stop.")
                        setServiceState(EmergencyCallStopReceived)
                    },
                    _ -> {
                        println("Unknown emergency call parameter.")
                    }
                }

            }
        }
        return new ServicePrimitive(Primitive.Continue)
    }

    setServiceState(ServiceState serviceState) {
        println("State change: old state = " + getServiceStateString(mState) + 
                ", new state = " + getServiceStateString(serviceState))
        mState = serviceState
    }

    ServicePrimitive rejectNotify(INotifyEvent notify) {
        let sessionContext = notify.getSessionContext
        let sfwEventDispatcher = sessionContext.getISfwEventDispatcher
        sfwEventDispatcher.requestEventTimeslot(this)

        mSessionContext = Some(notify.getSessionContext)
        setServiceState(WaitingToSendNotifyReject)
        return new ServicePrimitive(Primitive.Stop)
    }

    Option<SfwEventDescription> getEvent() {
        println("getEvent called on Emergency State Service service in state " +
                getServiceStateString(mState))
        
        match mState {
            WaitingToSendNotifyReject -> {
                setServiceState(Guard)
                return Some(createNotifyRejectEvent)
            },
            _ -> return None
        }
    }

    SfwEventDescription createNotifyRejectEvent() {
        if let Some(sessionContext) = mSessionContext {
            return new SfwEventDescription(
                sessionContext.getIncomingCallDialogId)
        }
        // Should not reach this line. Should panic.
    }
}

main() {
    let oamMoData = new OamMoData(false)
    let service = new EmergencyStateService(oamMoData)
    let eventDispatcher = new ISfwEventDispatcher
    let sessionContext = new ISessionContext(eventDispatcher)

    service.handleEvent(new INotifyEvent(sessionContext))
    service.handleEvent(new INotifyAcceptEvent(sessionContext))

    oamMoData.mLocked = true
    service.handleEvent(new INotifyEvent(sessionContext))

    if let Some(observer) = sessionContext.getISfwEventDispatcher.mObserver {
        observer.getEvent
    }
}

