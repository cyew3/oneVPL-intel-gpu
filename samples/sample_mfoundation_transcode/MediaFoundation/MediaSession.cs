#region Usings

using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Structure;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class MediaSession {
        private AsyncEventHandler asyncEventHandler;

        public MediaSession() {
            MFMediaSession = Com.Get((IMFAttributes _, out IMFMediaSession __) =>
                                     MFHelper.MFCreateMediaSession(_, out __), null);
            SubscribeToMediaSessionEvent(OnMediaEvent);
            State = MediaSessionState.Started;
        }

        public void Run(Topology topology) {
            MFMediaSession.SetTopology(0, topology.MFTopology);
        }

        public MediaSessionState State { get; private set; }
        public event EventHandler OnFinish;

        internal IMFMediaSession MFMediaSession { get; private set; }

        private void OnMediaEvent(uint eventType, int eventStatus) {
            if (eventStatus >= 0) {
                IMFClock clock;
                switch ((MediaEventType) eventType) {
                    case MediaEventType.MESessionTopologySet:
                        MFMediaSession.Start(new Guid(), new MediaSessionStartPosition(0));
                        break;
                    case MediaEventType.MESessionStarted:
                        // Get the presentation clock
                        MFMediaSession.GetClock(out clock);
//                        this.presentationClock = (IMFPresentationClock) clock;
//                        this.progressTimer.Start();
                        break;
                    case MediaEventType.MESessionEnded:
                        // Close the media session.
//                        this.presentationClock = null;
                        MFMediaSession.Close();
                        State = MediaSessionState.Finished;
                        break;
                    case MediaEventType.MESessionStopped:
                        // Close the media session.
//                        this.presentationClock = null;
//                        MediaSession.Close();
                        break;
                    case MediaEventType.MESessionClosed:
                        // Stop the progress timer
//                        this.presentationClock = null;
//                        this.progressTimer.Stop();

                        // Shutdown the media source and session 
                        MFMediaSession.Shutdown();
                        State = MediaSessionState.ShutDown;
                        if (OnFinish != null) {
                            OnFinish(this, new EventArgs());
                        }
                        break;
                }
            }
            else {
                MFMediaSession.Close();
                MFMediaSession.Shutdown();
                MFMediaSession = null;
                State = MediaSessionState.Failed;
            }
        }

        private void SubscribeToMediaSessionEvent(AsyncEventHandler.MediaEventHandler eventHandper) {
            asyncEventHandler = new AsyncEventHandler(this);
            asyncEventHandler.MediaEvent += eventHandper;
        }


        private class AsyncEventHandler : IMFAsyncCallback {
            private readonly MediaSession mediaSession;

            public AsyncEventHandler(MediaSession mediaSession) {
                this.mediaSession = mediaSession;
                this.mediaSession.MFMediaSession.BeginGetEvent(this, null);
            }

            public delegate void MediaEventHandler(uint eventType, int eventStatus);

            public event MediaEventHandler MediaEvent;

            public int GetParameters(out uint flags, out uint queue) {
                flags = 0;
                queue = 0;
                return Consts.E_NOTIMPL;
            }

            public int Invoke(IMFAsyncResult asyncResult) {
                IMFMediaEvent mediaEvent;
                mediaSession.MFMediaSession.EndGetEvent(asyncResult, out mediaEvent);

                uint type;
                mediaEvent.GetType(out type);

                int status;
                mediaEvent.GetStatus(out status);

                if (MediaEvent != null) {
                    MediaEvent(type, status);
                }

                if (mediaSession.State != MediaSessionState.ShutDown && mediaSession.State != MediaSessionState.Failed)
                    mediaSession.MFMediaSession.BeginGetEvent(this, null);

                return 0;
            }
        }
    }
}