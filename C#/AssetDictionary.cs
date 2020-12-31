// Created by Dennis 'DeeFeS' Schmidt 2020
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace DFSSolutions.AssetVariables
{
    public class AssetDictionary<TKey, TValue> : ScriptableObject
    {
        [SerializeField] private List<TKey> _Keys = new List<TKey>();
        [SerializeField] private List<TValue> _Values = new List<TValue>();

        public TValue this[TKey key]
        {
            get => _Values[_Keys.IndexOf(key)];
#if !(UNITY_EDITOR || DEVELOPMENT_BUILD)
            set => _Values[_Keys.IndexOf(key)] = value;
#else
            set
            {
                int idx = _Keys.IndexOf(key);
                if (idx == -1)
                {
                    Add(key, value);
                    Debug.LogFormat(LogType.Error, LogOption.None, this, "{0}: value wasn't found in dictionary. Entry has been added!", name);
                }
                else
                    _Values[idx] = value;
            }
#endif
        }

        public bool Add(TKey key, TValue value)
        {
            if (_Keys.Contains(key))
                return false;

            _Keys.Add(key);
            _Values.Add(value);
            return true;
        }

        public bool Remove(TKey key)
        {
            int idx = _Keys.IndexOf(key);
            if (idx == -1)
                return false;

            _Keys.RemoveAt(idx);
            _Values.RemoveAt(idx);
            return true;
        }

        public bool Get(TKey key, out TValue value)
        {
            int idx = _Keys.IndexOf(key);
            if (idx != -1)
            {
                value = _Values[idx];
                return true;
            }
            value = default;
            return false;
        }

        public Dictionary<TKey, TValue> ExportDictionary()
        {
            Dictionary<TKey, TValue> ret = new Dictionary<TKey, TValue>();
            for (int i = 0; i < _Keys.Count; i++)
                ret.Add(_Keys[i], _Values[i]);
            return ret;
        }
    }
}