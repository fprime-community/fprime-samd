module Samd21 {
    @ Measure time on the Samd21 through Arduino time
    passive component Samd21Time {

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        @ Command to set the time
        sync command SET_TIME(value: Fw.TimeValue)

        @ Port to retrieve time
        sync input port getTime: Fw.Time

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables command handling
        import Fw.Command

    }
}