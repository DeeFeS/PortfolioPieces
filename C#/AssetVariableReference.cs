// Created by Dennis 'DeeFeS' Schmidt 2020
using UnityEngine;

namespace DFSSolutions.AssetVariables
{
    [System.Serializable]
    public class AssetVariableReference<TType, TAsset>
        where TAsset : AssetVariable<TType>
    {
        [SerializeField] private TAsset _Asset = default;
        [SerializeField] private TType _Local = default;
        [SerializeField] private bool _UseLocal = false;
        [SerializeField] private bool _IsSubscribed = false;
        private AssetVariable<TType>.ValueChangeCallback _OnChange = default;

        /// <summary>
        /// Returns the currently referenced value depending on the state of UseLocal.
        /// </summary>
        public TType Value
        {
            get => _UseLocal ? _Local : _Asset.Value;
            set
            {
                if (_UseLocal && !_Local.Equals(value))
                {
                    _Local = value;
                    if (_IsSubscribed)
                        _OnChange?.Invoke(Value);
                }
                else
                    _Asset.Value = value;
            }
        }

        /// <summary>
        /// Try using '.Value' instead. Only access the asset directly if you know what you are doing.
        /// </summary>
        public TAsset Asset
        {
            get
            {
#if (UNITY_EDITOR || DEVELOPMENT_BUILD)
                if (_UseLocal)
                    Debug.LogFormat(LogType.Warning, LogOption.None, null, $"AssetReference<{typeof(TType).Name}> accessed asset although set to local.");
                if (!_Asset)
                    Debug.LogFormat(LogType.Warning, LogOption.None, null, $"AssetReference<{typeof(TType).Name}> accessed asset is NULL.");
#endif
                return _Asset;
            }
            set
            {
#if (UNITY_EDITOR || DEVELOPMENT_BUILD)
                if (_UseLocal)
                    Debug.LogFormat(LogType.Warning, LogOption.None, null, $"AssetReference<{typeof(TType).Name}> accessed asset although set to local.");
                if (!value)
                    Debug.LogFormat(LogType.Warning, LogOption.None, null, $"AssetReference<{typeof(TType).Name}> accessed asset gets set to NULL.");
#endif
                _Asset = value;
            }
        }

        /// <summary>
        /// Try using '.Value' instead. Only access the local directly if you know what you are doing.
        /// </summary>
        public TType Local
        {
            get
            {
#if (UNITY_EDITOR || DEVELOPMENT_BUILD)
                if (!_UseLocal)
                    Debug.LogFormat(LogType.Warning, LogOption.None, null, $"AssetReference<{typeof(TType).Name}> accessed local although set to asset.");
#endif
                return _Local;
            }
            set
            {
#if (UNITY_EDITOR || DEVELOPMENT_BUILD)
                if (!_UseLocal)
                    Debug.LogFormat(LogType.Warning, LogOption.None, null, $"AssetReference<{typeof(TType).Name}> set local value although set to asset.");
#endif
                _Local = value;
            }
        }

        /// <summary>
        /// Determines if local or asset is referenced when '.Value' is used.
        /// Returns wether or not OnChange gets invoked when the referenced value changed.
        /// </summary>
        public bool UseLocal
        {
            get => _UseLocal;
            set
            {
                if (_UseLocal == value)
                    return;

                _UseLocal = value;

                if (!_IsSubscribed)
                    return;

                if (_UseLocal)
                    _Asset.UnsubscribeFromValueChange(OnChange);
                else
                    _Asset.SubscribeToValueChange(OnChange);
            }
        }
        
        /// <summary>
        /// Determines if OnChange gets called when the referenced value changes.
        /// Returns wether or not OnChange gets invoked when the referenced value changed.
        /// </summary>
        public bool IsSubscribed
        {
            get => _IsSubscribed;
            set
            {
                if (_IsSubscribed == value)
                    return;

#if (UNITY_EDITOR || DEVELOPMENT_BUILD)
                if (_OnChange == null)
                    Debug.LogFormat(LogType.Warning, LogOption.None, null, "AssetReference<{0}> tries to {1} although no OnChange callback is NULL.", typeof(TType).Name, value ? "subscribe" : "unsubscribe");
#endif
                _IsSubscribed = value;

                if (_UseLocal)
                    return;

                if (_IsSubscribed)
                    _Asset.SubscribeToValueChange(OnChange);
                else
                    _Asset.UnsubscribeFromValueChange(OnChange);
            }
        }

        /// <summary>
        /// Returns the OnChange callback to manually invoke it.
        /// To listen to a change of the value, set '.IsSubscribed' to true.
        /// </summary>
        public AssetVariable<TType>.ValueChangeCallback OnChange
        {
            get => _OnChange;
            set
            {
                bool isSubsribed = _IsSubscribed;
                if(isSubsribed && _OnChange != null)
                    IsSubscribed = false;

                _OnChange = value;

                if (isSubsribed)
                    IsSubscribed = true;
            }
        }

        /// <summary>
        /// Performes a sanity check on the asset.
        /// </summary>
        /// <param name="ctx">The context of which the check is performed. Usually 'this'.</param>
        /// <param name="checkAlways">Check even when it's not referenced, i.e. when the local value is used.</param>
        public void CheckIfAssetIsSetup(Object ctx, bool checkAlways = false)
        {
            if (checkAlways || !_UseLocal)
                VariableNotSetUp.Check(_Asset, nameof(_Asset), ctx);
        }
    }
}