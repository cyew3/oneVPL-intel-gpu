#region Usings

using System;
using System.ComponentModel;
using System.Linq.Expressions;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.ViewModel {
    public class ViewModel : INotifyPropertyChanged {
        public event PropertyChangedEventHandler PropertyChanged;

        protected void OnPropertyChanged<T>(Expression<Func<T>> property) {
            var propertyName = ((MemberExpression) property.Body).Member.Name;
            var handler = PropertyChanged;
            if (handler != null) {
                handler(this, new PropertyChangedEventArgs(propertyName));
            }
        }
    }
}