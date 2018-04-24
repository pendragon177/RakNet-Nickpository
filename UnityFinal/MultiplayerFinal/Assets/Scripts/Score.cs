using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.Networking;


public class Score : NetworkBehaviour
{

    public const float points = 1;

    [SyncVar(hook = "OnScoreChange")]
    public float currentPoints = points;
    public Text pointsTracker;

    public void AddScore()
    {
        if (!isServer)
        {
            return;
        }

        currentPoints += Time.deltaTime;
  
    }

    void OnScoreChange(float currentPoints)
    {
        pointsTracker.text = "" + Mathf.Round(currentPoints);
    }
}